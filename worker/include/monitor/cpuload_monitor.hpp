#pragma once

#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"
#include <string>

namespace monitor {
class CpuLoadMonitor : public Monitor {
public:
  CpuLoadMonitor() {}
  void update(monitor::proto::MonitorInfo *monitor_info) override;
  void stop() override {}

private:
  float _load_avg_1;  // 1分钟平均负载
  float _load_avg_3;  // 3分钟平均负载
  float _load_avg_15; // 15分钟平均负载
};
} // namespace monitor