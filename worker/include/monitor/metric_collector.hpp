#pragma once

#include <memory>
#include <string>
#include <vector>

#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"

namespace monitor {

class MetricCollector {
 public:
  MetricCollector();
  ~MetricCollector();

  // 采集所有指标并填充到 MonitorInfo
  void collect_all(monitor::proto::MonitorInfo* monitor_info);

 private:
  std::vector<std::unique_ptr<Monitor>> _monitors;
  std::string _hostname;
};

}  // namespace monitor
