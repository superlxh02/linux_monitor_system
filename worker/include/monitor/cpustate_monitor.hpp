#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"

namespace monitor {
class CpuStatMonitor : public Monitor {
  struct CpuStat {
    std::string cpu_name;
    float user;
    float system;
    float idle;
    float nice;
    float io_wait;
    float irq;
    float soft_irq;
    float steal;
    float guest;
    float guest_nice;
  };

 public:
  CpuStatMonitor() {}
  void update(monitor::proto::MonitorInfo* monitor_info) override;
  void stop() override {}

 private:
    bool _device_missing = false;
    std::chrono::steady_clock::time_point _last_probe_time = std::chrono::steady_clock::time_point::min();
  std::unordered_map<std::string, struct CpuStat> _cpu_stat_map;
};

}  // namespace monitor
