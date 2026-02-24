#include "monitor/disk_monitor.hpp"
#include <ctime>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace monitor {
//磁盘结构体
struct DiskSample {
  uint64_t reads, writes, sectors_read, sectors_written;
  uint64_t read_time_ms, write_time_ms, io_in_progress, io_time_ms,
      weighted_io_time_ms;
};

static std::map<std::string, DiskSample> last_samples;
static std::map<std::string, double> last_time;

// 从/proc/diskstats读取磁盘信息并更新到monitor_info中
void DiskMonitor::update(monitor::proto::MonitorInfo* monitor_info) {
  std::ifstream ifs("/proc/diskstats");
  std::string line;
  double now = ::time(nullptr);

  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    int major, minor;
    std::string name;
    DiskSample curr{};
    iss >> major >> minor >> name >> curr.reads >> curr.writes >>
        curr.sectors_read >> curr.sectors_written >> curr.read_time_ms >>
        curr.write_time_ms >> curr.io_in_progress >> curr.io_time_ms >>
        curr.weighted_io_time_ms;
    if (name.find("loop") == 0 || name.find("ram") == 0)
      continue;  // 跳过虚拟盘

    auto* disk = monitor_info->add_disk_info();
    disk->set_name(name);
    disk->set_reads(curr.reads);
    disk->set_writes(curr.writes);
    disk->set_sectors_read(curr.sectors_read);
    disk->set_sectors_written(curr.sectors_written);
    disk->set_read_time_ms(curr.read_time_ms);
    disk->set_write_time_ms(curr.write_time_ms);
    disk->set_io_in_progress(curr.io_in_progress);
    disk->set_io_time_ms(curr.io_time_ms);
    disk->set_weighted_io_time_ms(curr.weighted_io_time_ms);

    // 速率/变化率计算
    auto it = last_samples.find(name);
    double dt = now - last_time[name];
    if (it != last_samples.end() && dt > 0) {
      const auto& last = it->second;
      double read_ios = curr.reads - last.reads;
      double write_ios = curr.writes - last.writes;
      double read_bytes = (curr.sectors_read - last.sectors_read) * 512.0;
      double write_bytes = (curr.sectors_written - last.sectors_written) * 512.0;
      double read_time = curr.read_time_ms - last.read_time_ms;
      double write_time = curr.write_time_ms - last.write_time_ms;
      double io_time = curr.io_time_ms - last.io_time_ms;

      disk->set_read_bytes_per_sec(read_bytes / dt);
      disk->set_write_bytes_per_sec(write_bytes / dt);
      disk->set_read_iops(read_ios / dt);
      disk->set_write_iops(write_ios / dt);
      disk->set_avg_read_latency_ms(read_ios > 0 ? read_time / read_ios : 0);
      disk->set_avg_write_latency_ms(write_ios > 0 ? write_time / write_ios : 0);
      disk->set_util_percent(io_time / (dt * 10.0));  // io_time单位ms
    } else {
      disk->set_read_bytes_per_sec(0);
      disk->set_write_bytes_per_sec(0);
      disk->set_read_iops(0);
      disk->set_write_iops(0);
      disk->set_avg_read_latency_ms(0);
      disk->set_avg_write_latency_ms(0);
      disk->set_util_percent(0);
    }
    last_samples[name] = curr;
    last_time[name] = now;
  }
  }


}