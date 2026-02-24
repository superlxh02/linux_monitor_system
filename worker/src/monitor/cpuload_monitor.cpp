#include "monitor/cpuload_monitor.hpp"
#include "fastlog/fastlog.hpp"
#include "monitor/monitor_struct.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

static bool read_load_from_proc(float *load_avg_1, float *load_avg_3,
                                float *load_avg_15) {
  FILE *fp = fopen("/proc/loadavg", "r");
  if (fp == nullptr) {
    fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to open /proc/loadavg");
    // fastlog::console.error("Failed to open /proc/loadavg");
    return false;
  }

  int ret = fscanf(fp, "%f %f %f", load_avg_1, load_avg_3, load_avg_15);
  fclose(fp);
  return ret == 3;
}

// 从内核模块或 /proc/loadavg 读取 CPU 负载信息并更新到 monitor_info 中
void CpuLoadMonitor::update(monitor::proto::MonitorInfo *monitor_info) {
  // 首先尝试从内核模块读取
  int fd = open("/dev/cpu_load_monitor", O_RDONLY);
  if (fd >= 0) {
    size_t load_size = sizeof(struct cpu_load);
    void *addr = mmap(nullptr, load_size, PROT_READ, MAP_SHARED, fd, 0);
    if (addr != MAP_FAILED) {
      struct cpu_load info;
      memcpy(&info, addr, load_size);

      auto cpu_load_msg = monitor_info->mutable_cpu_load();
      cpu_load_msg->set_load_avg_1(info.load_avg_1);
      cpu_load_msg->set_load_avg_3(info.load_avg_3);
      cpu_load_msg->set_load_avg_15(info.load_avg_15);

      munmap(addr, load_size);
      close(fd);
      return;
    }
    close(fd);
  }

  // 后备方案：从 /proc/loadavg 读取
  float load1, load3, load15;
  if (read_load_from_proc(&load1, &load3, &load15)) {
    auto cpu_load_msg = monitor_info->mutable_cpu_load();
    cpu_load_msg->set_load_avg_1(load1);
    cpu_load_msg->set_load_avg_3(load3);
    cpu_load_msg->set_load_avg_15(load15);
  }
}

} // namespace monitor