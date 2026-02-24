#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>

#include "monitor/monitor.hpp"

struct bpf_object;

namespace monitor {

/**
 * 基于 eBPF 的网络流量监控器
 * 
 * 使用 eBPF tracepoint 挂载到内核网络路径，
 * 实时统计每个网卡的收发流量。
 */
class NetEbpfMonitor : public Monitor {
 public:
  NetEbpfMonitor();
  ~NetEbpfMonitor() override;

  void update(monitor::proto::MonitorInfo* monitor_info) override;
  void stop() override;

  // 检查 eBPF 是否成功加载
  bool is_loaded() const { return _loaded; }

 private:
  // 初始化 eBPF 程序
  bool init_ebpf();
  
  // 清理 eBPF 资源
  void cleanup_ebpf();
  
  // 根据 ifindex 获取网卡名称
  std::string get_ifname(uint32_t ifindex);

  // 上一次采集的数据，用于计算速率
  struct NetStatCache {
    uint64_t rcv_bytes;
    uint64_t rcv_packets;
    uint64_t snd_bytes;
    uint64_t snd_packets;
    std::chrono::steady_clock::time_point timestamp;
  };

  std::unordered_map<uint32_t, NetStatCache> _cache;  // key: ifindex
  std::unordered_map<uint32_t, std::string> _ifname_cache;  // ifindex -> name
  std::vector<uint32_t> _attached_ifindexes_;  // 已附加 TC hook 的网卡
  
  struct bpf_object* _bpf_obj = nullptr;
  int _map_fd = -1;
  bool _loaded = false;
  
  std::chrono::steady_clock::time_point _last_update;
};

}  // namespace monitor
