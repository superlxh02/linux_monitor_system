#include "monitor/net_ebpf_monitor.hpp"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <linux/pkt_sched.h>
#include <unistd.h>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <dirent.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "fastlog/fastlog.hpp"
#include "monitor_info.pb.h"

// 包含生成的 skeleton 头文件
#include "monitor/net_stats.skel.h"

// 共享数据结构
extern "C" {
struct net_stats {
    uint64_t rcv_bytes;
    uint64_t rcv_packets;
    uint64_t snd_bytes;
    uint64_t snd_packets;
};
}

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

static int silent_libbpf_print(enum libbpf_print_level, const char*, va_list) {
    return 0;
}

// 获取系统所有网卡的 ifindex
static std::vector<uint32_t> get_all_ifindex() {
    std::vector<uint32_t> indexes;
    DIR* dir = opendir("/sys/class/net");
    if (!dir) return indexes;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.') continue;
        
        unsigned int ifindex = if_nametoindex(entry->d_name);
        if (ifindex > 0) {
            indexes.push_back(ifindex);
        }
    }
    closedir(dir);
    return indexes;
}

// TC qdisc 操作辅助函数
static int tc_qdisc_create_clsact(int ifindex) {
    // 使用 system 调用 tc 命令创建 clsact qdisc
    // 这是最简单可靠的方式
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "tc qdisc add dev $(cat /sys/class/net/*/ifindex | "
             "while read idx; do [ \"$idx\" = \"%d\" ] && "
             "basename $(dirname $(grep -l $idx /sys/class/net/*/ifindex)); done) "
             "clsact 2>/dev/null || true", ifindex);
    
    // 更简单的方式：通过 ifindex 获取设备名
    char ifname[IF_NAMESIZE];
    if (if_indextoname(ifindex, ifname) == nullptr) {
        return -1;
    }
    
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s clsact 2>/dev/null", ifname);
    return system(cmd);
}

static int tc_qdisc_delete_clsact(int ifindex) {
    char ifname[IF_NAMESIZE];
    if (if_indextoname(ifindex, ifname) == nullptr) {
        return -1;
    }
    
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s clsact 2>/dev/null", ifname);
    return system(cmd);
}

NetEbpfMonitor::NetEbpfMonitor() {
    _last_update = std::chrono::steady_clock::now();
    _loaded = init_ebpf();
    if (!_loaded) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("NetEbpfMonitor: eBPF unavailable (need root/CAP_BPF/CAP_NET_ADMIN and supported kernel), falling back to /proc/net/dev");
        // fastlog::console.error("NetEbpfMonitor: eBPF unavailable...");
    }
}

NetEbpfMonitor::~NetEbpfMonitor() {
    cleanup_ebpf();
}

bool NetEbpfMonitor::init_ebpf() {
    struct net_stats_bpf* skel = nullptr;
    int err;

    libbpf_set_print(silent_libbpf_print);

    // 打开并加载 BPF 程序
    skel = net_stats_bpf__open();
    if (!skel) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to open BPF skeleton");
        // fastlog::console.error("Failed to open BPF skeleton");
        return false;
    }

    err = net_stats_bpf__load(skel);
    if (err) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to load BPF program: {}", strerror(-err));
        // fastlog::console.error("Failed to load BPF program: {}", strerror(-err));
        net_stats_bpf__destroy(skel);
        return false;
    }

    // 获取 map fd
    _map_fd = bpf_map__fd(skel->maps.net_stats_map);
    if (_map_fd < 0) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to get map fd");
        // fastlog::console.error("Failed to get map fd");
        net_stats_bpf__destroy(skel);
        return false;
    }

    // 获取所有网卡并附加 TC hook
    auto ifindexes = get_all_ifindex();
    int ingress_fd = bpf_program__fd(skel->progs.tc_ingress);
    int egress_fd = bpf_program__fd(skel->progs.tc_egress);

    for (uint32_t ifindex : ifindexes) {
        char ifname[IF_NAMESIZE];
        if (if_indextoname(ifindex, ifname) == nullptr) continue;
        
        // 跳过 loopback
        if (strcmp(ifname, "lo") == 0) continue;

        // 创建 clsact qdisc
        tc_qdisc_create_clsact(ifindex);

        // 使用 libbpf 的 TC attach API
        LIBBPF_OPTS(bpf_tc_hook, hook,
            .ifindex = static_cast<int>(ifindex),
            .attach_point = BPF_TC_INGRESS,
        );
        
        // 创建 hook
        err = bpf_tc_hook_create(&hook);
        if (err && err != -EEXIST) {
            fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to create TC hook for {}: {}", ifname, strerror(-err));
            // fastlog::console.error("Failed to create TC hook for {}: {}", ifname, strerror(-err));
            continue;
        }

        // 附加 ingress 程序
        LIBBPF_OPTS(bpf_tc_opts, opts_in,
            .prog_fd = ingress_fd,
        );
        err = bpf_tc_attach(&hook, &opts_in);
        if (err) {
            fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to attach TC ingress for {}: {}", ifname, strerror(-err));
            // fastlog::console.error("Failed to attach TC ingress for {}: {}", ifname, strerror(-err));
        } else {
            _attached_ifindexes_.push_back(ifindex);
            fastlog::file::get_logger(kWorkerLoggerName)->info("Attached TC ingress to {}", ifname);
            // fastlog::console.info("Attached TC ingress to {}", ifname);
        }

        // 附加 egress 程序
        hook.attach_point = BPF_TC_EGRESS;
        LIBBPF_OPTS(bpf_tc_opts, opts_eg,
            .prog_fd = egress_fd,
        );
        err = bpf_tc_attach(&hook, &opts_eg);
        if (err) {
            fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to attach TC egress for {}: {}", ifname, strerror(-err));
            // fastlog::console.error("Failed to attach TC egress for {}: {}", ifname, strerror(-err));
        } else {
            fastlog::file::get_logger(kWorkerLoggerName)->info("Attached TC egress to {}", ifname);
            // fastlog::console.info("Attached TC egress to {}", ifname);
        }
    }

    // 保存 skeleton 指针
    _bpf_obj = reinterpret_cast<struct bpf_object*>(skel);

    if (_attached_ifindexes_.empty()) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("No interfaces attached");
        // fastlog::console.error("No interfaces attached");
        net_stats_bpf__destroy(skel);
        _bpf_obj = nullptr;
        return false;
    }

    fastlog::file::get_logger(kWorkerLoggerName)->info("NetEbpfMonitor: eBPF TC hook loaded successfully");
    // fastlog::console.info("NetEbpfMonitor: eBPF TC hook loaded successfully");
    return true;
}

void NetEbpfMonitor::cleanup_ebpf() {
    // 分离 TC hook
    for (uint32_t ifindex : _attached_ifindexes_) {
        LIBBPF_OPTS(bpf_tc_hook, hook,
            .ifindex = static_cast<int>(ifindex),
            .attach_point = BPF_TC_INGRESS,
        );
        
        LIBBPF_OPTS(bpf_tc_opts, opts);
        bpf_tc_detach(&hook, &opts);
        
        hook.attach_point = BPF_TC_EGRESS;
        bpf_tc_detach(&hook, &opts);
        
        // 可选：删除 clsact qdisc
        // tc_qdisc_delete_clsact(ifindex);
    }
    _attached_ifindexes_.clear();

    if (_bpf_obj) {
        net_stats_bpf__destroy(reinterpret_cast<struct net_stats_bpf*>(_bpf_obj));
        _bpf_obj = nullptr;
    }
    _map_fd = -1;
    _loaded = false;
}

std::string NetEbpfMonitor::get_ifname(uint32_t ifindex) {
    // 先查缓存
    auto it = _ifname_cache.find(ifindex);
    if (it != _ifname_cache.end()) {
        return it->second;
    }

    // 使用 if_indextoname 获取网卡名
    char ifname[IF_NAMESIZE];
    if (if_indextoname(ifindex, ifname) != nullptr) {
        std::string name(ifname);
        _ifname_cache[ifindex] = name;
        return name;
    }

    return "";
}

void NetEbpfMonitor::update(monitor::proto::MonitorInfo* monitor_info) {
    if (!monitor_info || !_loaded || _map_fd < 0) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - _last_update).count();
    
    // 避免除零
    if (duration == 0) {
        duration = 1;
    }

    // 遍历 BPF map 获取所有网卡统计
    uint32_t key = 0, next_key;
    struct net_stats stats;

    while (bpf_map_get_next_key(_map_fd, &key, &next_key) == 0) {
        if (bpf_map_lookup_elem(_map_fd, &next_key, &stats) == 0) {
            std::string ifname = get_ifname(next_key);
            if (ifname.empty() || ifname == "lo") {
                // 跳过未知网卡和 loopback
                key = next_key;
                continue;
            }

            auto* net_info = monitor_info->add_net_info();
            net_info->set_name(ifname);

            // 查找上次的缓存数据
            auto cache_it = _cache.find(next_key);
            if (cache_it != _cache.end()) {
                const auto& old = cache_it->second;
                auto old_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - old.timestamp).count();
                
                if (old_duration > 0) {
                    // 计算速率 (bytes/sec)
                    int64_t rcv_diff = stats.rcv_bytes - old.rcv_bytes;
                    int64_t snd_diff = stats.snd_bytes - old.snd_bytes;
                    int64_t rcv_pkt_diff = stats.rcv_packets - old.rcv_packets;
                    int64_t snd_pkt_diff = stats.snd_packets - old.snd_packets;

                    // 处理计数器溢出
                    if (rcv_diff < 0) rcv_diff = stats.rcv_bytes;
                    if (snd_diff < 0) snd_diff = stats.snd_bytes;
                    if (rcv_pkt_diff < 0) rcv_pkt_diff = stats.rcv_packets;
                    if (snd_pkt_diff < 0) snd_pkt_diff = stats.snd_packets;

                    // 转换为每秒速率
                    net_info->set_rcv_rate(rcv_diff * 1000 / old_duration);
                    net_info->set_send_rate(snd_diff * 1000 / old_duration);
                    net_info->set_rcv_packets_rate(rcv_pkt_diff * 1000 / old_duration);
                    net_info->set_send_packets_rate(snd_pkt_diff * 1000 / old_duration);
                }
            } else {
                // 首次采集，速率为 0
                net_info->set_rcv_rate(0);
                net_info->set_send_rate(0);
                net_info->set_rcv_packets_rate(0);
                net_info->set_send_packets_rate(0);
            }

            // 更新缓存
            _cache[next_key] = {
                stats.rcv_bytes,
                stats.rcv_packets,
                stats.snd_bytes,
                stats.snd_packets,
                now
            };
        }
        key = next_key;
    }

    _last_update = now;
}

void NetEbpfMonitor::stop() {
    cleanup_ebpf();
}

}  // namespace monitor
