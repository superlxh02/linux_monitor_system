#include "monitor/cpustate_monitor.hpp"
#include "monitor/monitor_struct.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include "monitor_info.pb.h"
#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

    // CPU 状态监控器实现,
    //通过读取 /dev/cpu_stat_monitor 获取 CPU 状态信息，并将其转换为 MonitorInfo 消息格式
void CpuStatMonitor::update(monitor::proto::MonitorInfo* monitor_info) {
    auto now = std::chrono::steady_clock::now();
    if (_device_missing &&
        now - _last_probe_time < std::chrono::seconds(30)) {
        return;
    }

    int fd = open("/dev/cpu_stat_monitor", O_RDONLY);
    if (fd < 0) {
        _last_probe_time = now;
        if (errno == ENOENT) {
            if (!_device_missing) {
                fastlog::file::get_logger(kWorkerLoggerName)->warn("Device /dev/cpu_stat_monitor not found, kernel module may be unavailable; cpu_stat collection disabled with periodic retry");
                // fastlog::console.warn("Device /dev/cpu_stat_monitor not found...");
                _device_missing = true;
            }
            return;
        }
        fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to open device /dev/cpu_stat_monitor: {}", strerror(errno));
        // fastlog::console.error("Failed to open device /dev/cpu_stat_monitor: {}", strerror(errno));
        return;
    }
    _device_missing = false;

    size_t stat_count = 128; // 假设最多128个CPU
    size_t stat_size = sizeof(struct cpu_stat) * stat_count;
    void* addr = mmap(nullptr, stat_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to mmap device /dev/cpu_stat_monitor: {}", strerror(errno));
        // fastlog::console.error("Failed to mmap device /dev/cpu_stat_monitor: {}", strerror(errno));
        close(fd);
        return;
    }

    struct cpu_stat* stats = static_cast<struct cpu_stat*>(addr);
    for (size_t i = 0; i < stat_count; ++i) {
        if (stats[i].cpu_name[0] == '\0') break;
        auto it = _cpu_stat_map.find(stats[i].cpu_name);
        if (it != _cpu_stat_map.end()) {
          struct CpuStat old = it->second;
          auto cpu_stat_msg = monitor_info->add_cpu_stat();
          float new_cpu_total_time = stats[i].user + stats[i].system +
                                     stats[i].idle + stats[i].nice +
                                     stats[i].iowait + stats[i].irq +
                                     stats[i].softirq + stats[i].steal;
          float old_cpu_total_time = old.user + old.system + old.idle + old.nice +
                                     old.io_wait + old.irq + old.soft_irq +
                                     old.steal;
          float new_cpu_busy_time = stats[i].user + stats[i].system +
                                  stats[i].nice + stats[i].irq +
                                  stats[i].softirq + stats[i].steal;
          float old_cpu_busy_time = old.user + old.system + old.nice + old.irq +
                                  old.soft_irq + old.steal;

          float cpu_percent = (new_cpu_busy_time - old_cpu_busy_time) /
                              (new_cpu_total_time - old_cpu_total_time) * 100.00;
          float cpu_user_percent = (stats[i].user - old.user) /
                                   (new_cpu_total_time - old_cpu_total_time) *
                                   100.00;
          float cpu_system_percent = (stats[i].system - old.system) /
                                     (new_cpu_total_time - old_cpu_total_time) *
                                     100.00;
          float cpu_nice_percent = (stats[i].nice - old.nice) /
                                   (new_cpu_total_time - old_cpu_total_time) *
                                   100.00;
          float cpu_idle_percent = (stats[i].idle - old.idle) /
                                   (new_cpu_total_time - old_cpu_total_time) *
                                   100.00;
          float cpu_io_wait_percent = (stats[i].iowait - old.io_wait) /
                                      (new_cpu_total_time - old_cpu_total_time) *
                                      100.00;
          float cpu_irq_percent = (stats[i].irq - old.irq) /
                                  (new_cpu_total_time - old_cpu_total_time) *
                                  100.00;
          float cpu_soft_irq_percent = (stats[i].softirq - old.soft_irq) /
                                       (new_cpu_total_time - old_cpu_total_time) *
                                       100.00;
          cpu_stat_msg->set_cpu_name(stats[i].cpu_name);
          cpu_stat_msg->set_cpu_percent(cpu_percent);
          cpu_stat_msg->set_usr_percent(cpu_user_percent);
          cpu_stat_msg->set_system_percent(cpu_system_percent);
          cpu_stat_msg->set_nice_percent(cpu_nice_percent);
          cpu_stat_msg->set_idle_percent(cpu_idle_percent);
          cpu_stat_msg->set_io_wait_percent(cpu_io_wait_percent);
          cpu_stat_msg->set_irq_percent(cpu_irq_percent);
          cpu_stat_msg->set_soft_irq_percent(cpu_soft_irq_percent);
        }
        // 将内核结构体数据转换为内部 CpuStat 结构体
        CpuStat& cached = _cpu_stat_map[stats[i].cpu_name];
        cached.cpu_name = stats[i].cpu_name;
        cached.user = stats[i].user;
        cached.nice = stats[i].nice;
        cached.system = stats[i].system;
        cached.idle = stats[i].idle;
        cached.io_wait = stats[i].iowait;
        cached.irq = stats[i].irq;
        cached.soft_irq = stats[i].softirq;
        cached.steal = stats[i].steal;
        cached.guest = stats[i].guest;
        cached.guest_nice = stats[i].guest_nice;
    }

    munmap(addr, stat_size);
    close(fd);
    return;
}
}  // namespace monitor