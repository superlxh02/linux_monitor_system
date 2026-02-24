#include "monitor/metric_collector.hpp"

#include <unistd.h>

#include <memory>

#include "monitor/cpuload_monitor.hpp"
#include "monitor/cpusoftirq_monitor.hpp"
#include "monitor/cpustate_monitor.hpp"
#include "monitor/disk_monitor.hpp"
#include "monitor/memory_monitor.hpp"
#include "monitor/hostinfo_monitor.hpp"
#include "monitor/net_monitor.hpp"
#include "monitor/net_ebpf_monitor.hpp"

namespace monitor {

MetricCollector::MetricCollector() {
  // 获取主机名
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    _hostname = hostname;
  } else {
    _hostname = "unknown";
  }

  // 初始化所有监控器
  _monitors.push_back(std::make_unique<CpuLoadMonitor>());
  _monitors.push_back(std::make_unique<CpuStatMonitor>());
  _monitors.push_back(std::make_unique<CpuSoftIrqMonitor>());
  _monitors.push_back(std::make_unique<MemoryMonitor>());
  _monitors.push_back(std::make_unique<NetEbpfMonitor>());
  _monitors.push_back(std::make_unique<DiskMonitor>());
  _monitors.push_back(std::make_unique<HostInfoMonitor>());
}

MetricCollector::~MetricCollector() {
  for (auto& monitor : _monitors) {
    monitor->stop();
  }
}

void MetricCollector::collect_all(monitor::proto::MonitorInfo* monitor_info) {
  if (!monitor_info) {
    return;
  }

  // 设置主机名
  monitor_info->set_name(_hostname);

  // 调用每个监控器的 UpdateOnce 方法
  for (auto& monitor : _monitors) {
    monitor->update(monitor_info);
  }
}

}  // namespace monitor
