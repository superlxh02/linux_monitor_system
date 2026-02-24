#pragma once

#include <string>
#include <unordered_map>
#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"

namespace monitor {

class DiskMonitor : public Monitor {
 public:
  DiskMonitor() {}
  void update(monitor::proto::MonitorInfo* monitor_info) override;
  void stop() override {}
};

}  // namespace monitor
