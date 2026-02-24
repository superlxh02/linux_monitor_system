#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"

namespace monitor {
class CpuSoftIrqMonitor : public Monitor {
  struct SoftIrq {
    std::string cpu_name;
    int64_t hi;
    int64_t timer;
    int64_t net_tx;
    int64_t net_rx;
    int64_t block;
    int64_t irq_poll;
    int64_t tasklet;
    int64_t sched;
    int64_t hrtimer;
    int64_t rcu;
    std::chrono::steady_clock::time_point timepoint;
  };

 public:
  CpuSoftIrqMonitor() {}
  void update(monitor::proto::MonitorInfo* monitor_info) override;
  void stop() override {}

 private:
    bool _device_missing = false;
    std::chrono::steady_clock::time_point _last_probe_time = std::chrono::steady_clock::time_point::min();
  std::unordered_map<std::string, struct SoftIrq> _cpu_softirqs;
};

}  // namespace monitor