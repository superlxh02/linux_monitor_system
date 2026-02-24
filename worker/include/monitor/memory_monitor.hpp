#pragma once

#include "monitor/monitor.hpp"
#include "monitor_info.pb.h"
#include <string>
#include <unordered_map>

namespace monitor {
class MemoryMonitor : public Monitor {
public:
  MemoryMonitor() {}
  void update(monitor::proto::MonitorInfo *monitor_info) override;
  void stop() override{}

private:
// 内部结构体，用于存储内存信息
  struct mem_info {
    int64_t total;
    int64_t free;
    int64_t avail;
    int64_t buffers;
    int64_t cached;
    int64_t swap_cached;
    int64_t active;
    int64_t in_active;
    int64_t active_anon;
    int64_t inactive_anon;
    int64_t active_file;
    int64_t inactive_file;
    int64_t dirty;
    int64_t writeback;
    int64_t anon_pages;
    int64_t mapped;
    int64_t kReclaimable;
    int64_t sReclaimable;
    int64_t sUnreclaim;
  };
};
} // namespace monitor