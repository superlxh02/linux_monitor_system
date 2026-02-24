#include "rpc/monitor_pusher.hpp"

#include <iostream>
#include <chrono>
#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}
MonitorPusher::MonitorPusher(const std::string& manager_address,
                             int interval_seconds)
    : _manager_address(manager_address),
      _interval_seconds(interval_seconds),
      _running(false) {
  // 创建 gRPC channel 和 stub
  auto channel = grpc::CreateChannel(manager_address,
                                     grpc::InsecureChannelCredentials());
  _stub = monitor::proto::GrpcManager::NewStub(channel);

  // 创建指标采集器
  _collector = std::make_unique<MetricCollector>();
}

MonitorPusher::~MonitorPusher() {
  stop();
}

void MonitorPusher::start() {
  if (_running) {
    return;
  }
  _running = true;
  _thread = std::make_unique<std::thread>(&MonitorPusher::push_for_loop, this);
  fastlog::file::get_logger(kWorkerLoggerName)->info("MonitorPusher started, pushing to {} every {} seconds", _manager_address, _interval_seconds);
}

void MonitorPusher::stop() {
  _running = false;
  if (_thread && _thread->joinable()) {
    _thread->join();
  }
}

void MonitorPusher::push_for_loop() {
  while (_running) {
    if (!push_once()) {
      fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to push monitor data to {}", _manager_address);
      // fastlog::console.error("Failed to push monitor data to {}", _manager_address);
    }

    // 等待指定间隔
    for (int i = 0; i < _interval_seconds && _running   ; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

bool MonitorPusher::push_once() {
  // 采集监控数据
  monitor::proto::MonitorInfo info;
  _collector->collect_all(&info);

  // 打印采集到的所有指标
  fastlog::file::get_logger(kWorkerLoggerName)->info("\n================== Collected Metrics ==================\n");
  // 主机信息
  if (info.has_host_info()) {
    fastlog::file::get_logger(kWorkerLoggerName)->info("[Host] Hostname: {}, IP: {}", info.host_info().hostname(), info.host_info().ip_address());
  }
  
  // CPU 统计信息 - 所有核心
  fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- CPU Statistics ---");
  for (int i = 0; i < info.cpu_stat_size(); ++i) {
    const auto& cpu = info.cpu_stat(i);
    fastlog::file::get_logger(kWorkerLoggerName)->info("[{}] Total: {}%, User: {}%, System: {}%, Nice: {}%, Idle: {}%, IOWait: {}%, IRQ: {}%, SoftIRQ: {}%",
                          cpu.cpu_name(), cpu.cpu_percent(), cpu.usr_percent(), cpu.system_percent(),
                          cpu.nice_percent(), cpu.idle_percent(), cpu.io_wait_percent(),
                          cpu.irq_percent(), cpu.soft_irq_percent());
  }
  
  // CPU 负载
  if (info.has_cpu_load()) {
    fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- CPU Load ---");
    fastlog::file::get_logger(kWorkerLoggerName)->info("[Load] 1min: {}, 5min: {}, 15min: {}",
                          info.cpu_load().load_avg_1(), info.cpu_load().load_avg_3(), info.cpu_load().load_avg_15());
  }
  
  // 内存信息 - 所有字段
  if (info.has_mem_info()) {
    const auto& mem = info.mem_info();
    fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- Memory Info ---");
    fastlog::file::get_logger(kWorkerLoggerName)->info("[Memory] Used: {}%", mem.used_percent());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  Total: {} MB, Free: {} MB, Avail: {} MB", mem.total(), mem.free(), mem.avail());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  Buffers: {} MB, Cached: {} MB, SwapCached: {} MB", mem.buffers(), mem.cached(), mem.swap_cached());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  Active: {} MB, Inactive: {} MB", mem.active(), mem.inactive());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  ActiveAnon: {} MB, InactiveAnon: {} MB", mem.active_anon(), mem.inactive_anon());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  ActiveFile: {} MB, InactiveFile: {} MB", mem.active_file(), mem.inactive_file());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  Dirty: {} MB, Writeback: {} MB", mem.dirty(), mem.writeback());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  AnonPages: {} MB, Mapped: {} MB", mem.anon_pages(), mem.mapped());
    fastlog::file::get_logger(kWorkerLoggerName)->info("  KReclaimable: {} MB, SReclaimable: {} MB, SUnreclaim: {} MB",
                          mem.kreclaimable(), mem.sreclaimable(), mem.sunreclaim());
  }
  
  // 网络信息 - 所有网卡所有字段
  if (info.net_info_size() > 0) {
    fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- Network Info ---");
    for (int i = 0; i < info.net_info_size(); ++i) {
      const auto& net = info.net_info(i);
      fastlog::file::get_logger(kWorkerLoggerName)->info("[{}]", net.name());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Recv: {} B/s ({} pkt/s)", net.rcv_rate(), net.rcv_packets_rate());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Send: {} B/s ({} pkt/s)", net.send_rate(), net.send_packets_rate());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Errors(in/out): {}/{}, Drops(in/out): {}/{}",
                            net.err_in(), net.err_out(), net.drop_in(), net.drop_out());
    }
  }
  
  // 磁盘信息 - 所有磁盘所有字段
  if (info.disk_info_size() > 0) {
    fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- Disk Info ---");
    for (int i = 0; i < info.disk_info_size(); ++i) {
      const auto& disk = info.disk_info(i);
      fastlog::file::get_logger(kWorkerLoggerName)->info("[{}]", disk.name());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Read: {} KB/s, IOPS: {}, Latency: {} ms",
                            disk.read_bytes_per_sec() / 1024.0, disk.read_iops(), disk.avg_read_latency_ms());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Write: {} KB/s, IOPS: {}, Latency: {} ms",
                            disk.write_bytes_per_sec() / 1024.0, disk.write_iops(), disk.avg_write_latency_ms());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Util: {}%, IO_InProgress: {}", disk.util_percent(), disk.io_in_progress());
      fastlog::file::get_logger(kWorkerLoggerName)->info("  Reads: {}, Writes: {}, SectorsRead: {}, SectorsWritten: {}",
                            disk.reads(), disk.writes(), disk.sectors_read(), disk.sectors_written());
    }
  }
  
  // 软中断信息 - 所有 CPU 核心
  if (info.soft_irq_size() > 0) {
    fastlog::file::get_logger(kWorkerLoggerName)->info("\n--- SoftIRQ Info ---");
    for (int i = 0; i < info.soft_irq_size(); ++i) {
      const auto& sirq = info.soft_irq(i);
      fastlog::file::get_logger(kWorkerLoggerName)->info("[{}] HI: {}, TIMER: {}, NET_TX: {}, NET_RX: {}, BLOCK: {}, IRQ_POLL: {}, TASKLET: {}, SCHED: {}, HRTIMER: {}, RCU: {}",
                            sirq.cpu(), sirq.hi(), sirq.timer(), sirq.net_tx(), sirq.net_rx(),
                            sirq.block(), sirq.irq_poll(), sirq.tasklet(), sirq.sched(),
                            sirq.hrtimer(), sirq.rcu());
    }
  }
  
  fastlog::file::get_logger(kWorkerLoggerName)->info("========================================================\n");

  // 推送数据
  grpc::ClientContext context;
  google::protobuf::Empty response;

  grpc::Status status = _stub->SetMonitorInfo(&context, info, &response);

  if (status.ok()) {
    fastlog::file::get_logger(kWorkerLoggerName)->info(">>> Pushed monitor data to {} successfully <<<", _manager_address);
    return true;
  } else {
    fastlog::file::get_logger(kWorkerLoggerName)->error(">>> Push failed: {} <<<", status.error_message());
    // fastlog::console.error(">>> Push failed: {} <<<", status.error_message());
    return false;
  }
}

}