#include "monitor/cpusoftirq_monitor.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cstring>
#include "monitor/monitor_struct.h"
#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

// 设备路径
static const char* DEVICE_PATH = "/dev/cpu_softirq_monitor";

// 最大 CPU 数量（与内核模块一致）
static const size_t MAX_CPUS = 256;

void CpuSoftIrqMonitor::update(monitor::proto::MonitorInfo* monitor_info) {
  auto probe_now = std::chrono::steady_clock::now();
  if (_device_missing &&
      probe_now - _last_probe_time < std::chrono::seconds(30)) {
    return;
  }

  // 打开设备文件
  int fd = open(DEVICE_PATH, O_RDONLY);
  if (fd < 0) {
    _last_probe_time = probe_now;
    if (errno == ENOENT) {
      if (!_device_missing) {
        fastlog::file::get_logger(kWorkerLoggerName)->warn("Device {} not found, kernel module may be unavailable; softirq collection disabled with periodic retry", DEVICE_PATH);
        // fastlog::console.warn("Device {} not found...", DEVICE_PATH);
        _device_missing = true;
      }
      return;
    }
    fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to open device {}: {}", DEVICE_PATH, strerror(errno));
    // fastlog::console.error("Failed to open device {}: {}", DEVICE_PATH, strerror(errno));
    return;
  }
  _device_missing = false;

  // 计算映射大小
  size_t map_size = sizeof(struct softirq_stat) * MAX_CPUS;

  // 映射内核内存到用户空间
  void* addr = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    close(fd);
    fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to mmap device {}: {}", DEVICE_PATH, strerror(errno));
    // fastlog::console.error("Failed to mmap device {}: {}", DEVICE_PATH, strerror(errno));
    return;
  }

  struct softirq_stat* stats = static_cast<struct softirq_stat*>(addr);
  auto now = std::chrono::steady_clock::now();

  // 遍历所有 CPU 的软中断统计数据
  for (size_t i = 0; i < MAX_CPUS; ++i) {
    // 检查是否到达数据末尾
    if (stats[i].cpu_name[0] == '\0') {
      break;
    }

    std::string cpu_name(stats[i].cpu_name);

    // 查找之前的采样数据
    auto it = _cpu_softirqs.find(cpu_name);

    // 创建 protobuf 消息
    auto* softirq_msg = monitor_info->add_soft_irq();
    softirq_msg->set_cpu(cpu_name);

    if (it != _cpu_softirqs.end()) {
      // 计算时间差（秒）
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - it->second.timepoint)
                          .count();
      double seconds = duration / 1000.0;

      if (seconds > 0) {
        // 计算每秒的软中断频率
        softirq_msg->set_hi(
            static_cast<int64_t>((stats[i].hi - it->second.hi) / seconds));
        softirq_msg->set_timer(
            static_cast<int64_t>((stats[i].timer - it->second.timer) / seconds));
        softirq_msg->set_net_tx(
            static_cast<int64_t>((stats[i].net_tx - it->second.net_tx) / seconds));
        softirq_msg->set_net_rx(
            static_cast<int64_t>((stats[i].net_rx - it->second.net_rx) / seconds));
        softirq_msg->set_block(
            static_cast<int64_t>((stats[i].block - it->second.block) / seconds));
        softirq_msg->set_irq_poll(
            static_cast<int64_t>((stats[i].irq_poll - it->second.irq_poll) / seconds));
        softirq_msg->set_tasklet(
            static_cast<int64_t>((stats[i].tasklet - it->second.tasklet) / seconds));
        softirq_msg->set_sched(
            static_cast<int64_t>((stats[i].sched - it->second.sched) / seconds));
        softirq_msg->set_hrtimer(
            static_cast<int64_t>((stats[i].hrtimer - it->second.hrtimer) / seconds));
        softirq_msg->set_rcu(
            static_cast<int64_t>((stats[i].rcu - it->second.rcu) / seconds));
      } else {
        // 时间差为 0，使用原始值
        softirq_msg->set_hi(stats[i].hi);
        softirq_msg->set_timer(stats[i].timer);
        softirq_msg->set_net_tx(stats[i].net_tx);
        softirq_msg->set_net_rx(stats[i].net_rx);
        softirq_msg->set_block(stats[i].block);
        softirq_msg->set_irq_poll(stats[i].irq_poll);
        softirq_msg->set_tasklet(stats[i].tasklet);
        softirq_msg->set_sched(stats[i].sched);
        softirq_msg->set_hrtimer(stats[i].hrtimer);
        softirq_msg->set_rcu(stats[i].rcu);
      }
    } else {
      // 首次采集，使用原始累计值
      softirq_msg->set_hi(stats[i].hi);
      softirq_msg->set_timer(stats[i].timer);
      softirq_msg->set_net_tx(stats[i].net_tx);
      softirq_msg->set_net_rx(stats[i].net_rx);
      softirq_msg->set_block(stats[i].block);
      softirq_msg->set_irq_poll(stats[i].irq_poll);
      softirq_msg->set_tasklet(stats[i].tasklet);
      softirq_msg->set_sched(stats[i].sched);
      softirq_msg->set_hrtimer(stats[i].hrtimer);
      softirq_msg->set_rcu(stats[i].rcu);
    }

    // 保存当前采样数据用于下次计算
    SoftIrq& cached = _cpu_softirqs[cpu_name];
    cached.cpu_name = cpu_name;
    cached.hi = stats[i].hi;
    cached.timer = stats[i].timer;
    cached.net_tx = stats[i].net_tx;
    cached.net_rx = stats[i].net_rx;
    cached.block = stats[i].block;
    cached.irq_poll = stats[i].irq_poll;
    cached.tasklet = stats[i].tasklet;
    cached.sched = stats[i].sched;
    cached.hrtimer = stats[i].hrtimer;
    cached.rcu = stats[i].rcu;
    cached.timepoint = now;
  }

  // 解除映射并关闭文件
  munmap(addr, map_size);
  close(fd);
}

}  // namespace monitor
