#include "host_manager.hpp"

#include <iostream>
#include <map>

#include "fastlog/fastlog.hpp"

#include <mysql/mysql.h>

namespace monitor {

namespace {
constexpr char kManagerLoggerName[] = "manager_file_logger";
const char* MYSQL_HOST = "127.0.0.1";
const char* MYSQL_USER = "monitor";
const char* MYSQL_PASS = "monitor123";
const char* MYSQL_DB = "monitor_db";

// 用于详细表变化率计算的历史数据
struct NetDetailSample {
  float rcv_bytes_rate = 0;
  float rcv_packets_rate = 0;
  float snd_bytes_rate = 0;
  float snd_packets_rate = 0;
  // 错误和丢弃统计
  uint64_t err_in = 0;
  uint64_t err_out = 0;
  uint64_t drop_in = 0;
  uint64_t drop_out = 0;
};

struct SoftIrqSample {
  float hi = 0, timer = 0, net_tx = 0, net_rx = 0, block = 0;
  float irq_poll = 0, tasklet = 0, sched = 0, hrtimer = 0, rcu = 0;
};

struct MemDetailSample {
  float total = 0, free = 0, avail = 0, buffers = 0, cached = 0;
  float swap_cached = 0, active = 0, inactive = 0;
  float active_anon = 0, inactive_anon = 0, active_file = 0, inactive_file = 0;
  float dirty = 0, writeback = 0, anon_pages = 0, mapped = 0;
  float kreclaimable = 0, sreclaimable = 0, sunreclaim = 0;
};

struct DiskDetailSample {
  float read_bytes_per_sec = 0;
  float write_bytes_per_sec = 0;
  float read_iops = 0;
  float write_iops = 0;
  float avg_read_latency_ms = 0;
  float avg_write_latency_ms = 0;
  float util_percent = 0;
};

// 历史数据存储 (host_name -> net_name/cpu_name/disk_name -> sample)
static std::map<std::string, std::map<std::string, NetDetailSample>> last_net_samples;
static std::map<std::string, std::map<std::string, SoftIrqSample>> last_softirq_samples;
static std::map<std::string, MemDetailSample> last_mem_samples;
static std::map<std::string, std::map<std::string, DiskDetailSample>> last_disk_samples;

}  // namespace

// 用于网络速率计算的采样数据
struct NetSample {
  double last_in_bytes = 0;
  double last_out_bytes = 0;
  std::chrono::system_clock::time_point last_time;
};
static std::map<std::string, NetSample> net_samples;

// 用于变化率计算的性能采样数据
struct PerfSample {
  float cpu_percent = 0, usr_percent = 0, system_percent = 0;
  float nice_percent = 0, idle_percent = 0, io_wait_percent = 0;
  float irq_percent = 0, soft_irq_percent = 0;
  float steal_percent = 0, guest_percent = 0, guest_nice_percent = 0;
  float load_avg_1 = 0, load_avg_3 = 0, load_avg_15 = 0;
  float mem_used_percent = 0, mem_total = 0, mem_free = 0, mem_avail = 0;
  float net_in_rate = 0, net_out_rate = 0;
  float score = 0;
};
static std::map<std::string, PerfSample> last_perf_samples;

HostManager::HostManager() : _running(false) {
}

HostManager::~HostManager() {
  stop();
}

void HostManager::start() {
  _running = true;
  _thread = std::make_unique<std::thread>(&HostManager::process_for_loop, this);
}

void HostManager::stop() {
  _running = false;
  if (_thread && _thread->joinable()) {
    _thread->join();
  }
}

void HostManager::process_for_loop() {
  while (_running) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    auto now = std::chrono::system_clock::now();
    std::lock_guard<std::mutex> lock(_mtx);
    for (auto it = _host_scores.begin(); it != _host_scores.end();) {
      auto age = std::chrono::duration_cast<std::chrono::seconds>(
          now - it->second.timestamp).count();
      if (age > 60) {
        fastlog::file::get_logger(kManagerLoggerName)->debug("Removing stale host: {}", it->first);
        it = _host_scores.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void HostManager::on_data_received(const monitor::proto::MonitorInfo& info) {
  // 构建服务器唯一标识: hostname_ip
  std::string host_name;
  if (info.has_host_info()) {
    const auto& host_info = info.host_info();
    std::string hostname = host_info.hostname();
    std::string ip = host_info.ip_address();
    
    if (!hostname.empty() && !ip.empty()) {
      host_name = hostname + "_" + ip;  // 格式: hostname_192.168.1.100
    } else if (!hostname.empty()) {
      host_name = hostname;
    } else if (!ip.empty()) {
      host_name = ip;
    }
  }
  
  // 兼容旧版本：如果 host_info 为空，使用 name 字段
  if (host_name.empty()) {
    host_name = info.name();
  }
  
  if (host_name.empty()) {
    fastlog::file::get_logger(kManagerLoggerName)->error("Received data with empty server identifier");
    return;
  }

  double score = calc_scores(info);
  auto now = std::chrono::system_clock::now();

  // 网络速率计算
  double net_in_rate = 0, net_out_rate = 0;
  if (info.net_info_size() > 0) {
    net_in_rate = info.net_info(0).rcv_rate() / (1024.0 * 1024.0);
    net_out_rate = info.net_info(0).send_rate() / (1024.0 * 1024.0);
  }

  // 当前采样
  PerfSample curr;
  if (info.cpu_stat_size() > 0) {
    const auto& cpu = info.cpu_stat(0);
    curr.cpu_percent = cpu.cpu_percent();
    curr.usr_percent = cpu.usr_percent();
    curr.system_percent = cpu.system_percent();
    curr.nice_percent = cpu.nice_percent();
    curr.idle_percent = cpu.idle_percent();
    curr.io_wait_percent = cpu.io_wait_percent();
    curr.irq_percent = cpu.irq_percent();
    curr.soft_irq_percent = cpu.soft_irq_percent();
  }
  if (info.has_cpu_load()) {
    curr.load_avg_1 = info.cpu_load().load_avg_1();
    curr.load_avg_3 = info.cpu_load().load_avg_3();
    curr.load_avg_15 = info.cpu_load().load_avg_15();
  }
  if (info.has_mem_info()) {
    curr.mem_used_percent = info.mem_info().used_percent();
    curr.mem_total = info.mem_info().total();
    curr.mem_free = info.mem_info().free();
    curr.mem_avail = info.mem_info().avail();
  }
  curr.net_in_rate = net_in_rate;
  curr.net_out_rate = net_out_rate;
  curr.score = score;

  // 变化率计算
  PerfSample last = last_perf_samples[host_name];
  auto rate = [](float now_val, float last_val) -> float {
    if (last_val == 0) return 0;
    return (now_val - last_val) / last_val;
  };

  float cpu_percent_rate = rate(curr.cpu_percent, last.cpu_percent);
  float usr_percent_rate = rate(curr.usr_percent, last.usr_percent);
  float system_percent_rate = rate(curr.system_percent, last.system_percent);
  float nice_percent_rate = rate(curr.nice_percent, last.nice_percent);
  float idle_percent_rate = rate(curr.idle_percent, last.idle_percent);
  float io_wait_percent_rate = rate(curr.io_wait_percent, last.io_wait_percent);
  float irq_percent_rate = rate(curr.irq_percent, last.irq_percent);
  float soft_irq_percent_rate = rate(curr.soft_irq_percent, last.soft_irq_percent);
  float load_avg_1_rate = rate(curr.load_avg_1, last.load_avg_1);
  float load_avg_3_rate = rate(curr.load_avg_3, last.load_avg_3);
  float load_avg_15_rate = rate(curr.load_avg_15, last.load_avg_15);
  float mem_used_percent_rate = rate(curr.mem_used_percent, last.mem_used_percent);
  float mem_total_rate = rate(curr.mem_total, last.mem_total);
  float mem_free_rate = rate(curr.mem_free, last.mem_free);
  float mem_avail_rate = rate(curr.mem_avail, last.mem_avail);
  float net_in_rate_rate = rate(curr.net_in_rate, last.net_in_rate);
  float net_out_rate_rate = rate(curr.net_out_rate, last.net_out_rate);

  last_perf_samples[host_name] = curr;

  {
    std::lock_guard<std::mutex> lock(_mtx);
    _host_scores[host_name] = HostScore{info, score, now};
  }

  // 写入所有表
  write_to_mysql(host_name, HostScore{info, score, now}, net_in_rate,
                 net_out_rate, cpu_percent_rate, usr_percent_rate,
                 system_percent_rate, nice_percent_rate, idle_percent_rate,
                 io_wait_percent_rate, irq_percent_rate, soft_irq_percent_rate,
                 0, 0, 0, load_avg_1_rate, load_avg_3_rate, load_avg_15_rate,
                 mem_used_percent_rate, mem_total_rate, mem_free_rate,
                 mem_avail_rate, net_in_rate_rate, net_out_rate_rate, 0, 0);

  fastlog::file::get_logger(kManagerLoggerName)->debug("\n================== Received Data ==================");
  fastlog::file::get_logger(kManagerLoggerName)->debug("Server: {}, Score: {}", host_name, score);
  
  // CPU 详细信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- CPU ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Usage: {}%, User: {}%, System: {}%", curr.cpu_percent,
                         curr.usr_percent, curr.system_percent);
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Nice: {}%, Idle: {}%, IOWait: {}%", curr.nice_percent,
                         curr.idle_percent, curr.io_wait_percent);
  fastlog::file::get_logger(kManagerLoggerName)->debug("  IRQ: {}%, SoftIRQ: {}%", curr.irq_percent,
                         curr.soft_irq_percent);
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Load: {}/{}/{}", curr.load_avg_1, curr.load_avg_3,
                         curr.load_avg_15);
  
  // 内存详细信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- Memory ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Used: {}%, Total: {} MB", curr.mem_used_percent,
                         curr.mem_total);
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Free: {} MB, Avail: {} MB", curr.mem_free,
                         curr.mem_avail);
  
  // 网络详细信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- Network ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  In: {} B/s, Out: {} B/s", net_in_rate * 1024 * 1024,
                         net_out_rate * 1024 * 1024);
  for (int i = 0; i < info.net_info_size(); ++i) {
    const auto& net = info.net_info(i);
    fastlog::file::get_logger(kManagerLoggerName)->debug("  [{}] Recv: {} B/s, Send: {} B/s, Drops: {}/{}",
                           net.name(), net.rcv_rate(), net.send_rate(),
                           net.drop_in(), net.drop_out());
  }
  
  // 磁盘详细信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- Disk ---");
  float max_disk_util = 0;
  for (int i = 0; i < info.disk_info_size(); ++i) {
    const auto& disk = info.disk_info(i);
    fastlog::file::get_logger(kManagerLoggerName)->debug(
        "  [{}] Read: {} KB/s, Write: {} KB/s, Util: {}%", disk.name(),
        disk.read_bytes_per_sec() / 1024.0, disk.write_bytes_per_sec() / 1024.0,
        disk.util_percent());
    if (disk.util_percent() > max_disk_util) max_disk_util = disk.util_percent();
  }
  if (info.disk_info_size() == 0) {
    fastlog::file::get_logger(kManagerLoggerName)->debug("  No disk data");
  }
  
  // 软中断信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- SoftIRQ ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  CPU cores with softirq data: {}", info.soft_irq_size());
  
  // 变化率信息
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- Change Rates ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  CPU: {}%, Mem: {}%, Load: {}%",
                         cpu_percent_rate * 100, mem_used_percent_rate * 100,
                         load_avg_1_rate * 100);
  fastlog::file::get_logger(kManagerLoggerName)->debug("  NetIn: {}%, NetOut: {}%", net_in_rate_rate * 100,
                         net_out_rate_rate * 100);
  
  fastlog::file::get_logger(kManagerLoggerName)->debug("\n--- Database ---");
  fastlog::file::get_logger(kManagerLoggerName)->debug("  Data saved to MySQL (monitor_db)");
  fastlog::file::get_logger(kManagerLoggerName)->debug("====================================================\n");
}

std::unordered_map<std::string, HostScore> HostManager::get_all_host_scores() {
  std::lock_guard<std::mutex> lock(_mtx);
  return _host_scores;
}

std::string HostManager::get_best_host() {
  std::lock_guard<std::mutex> lock(_mtx);
  std::string best_host;
  double best_score = -1;
  for (const auto& [host, data] : _host_scores) {
    if (data.score > best_score) {
      best_score = data.score;
      best_host = host;
    }
  }
  return best_host;
}


double HostManager::calc_scores(const monitor::proto::MonitorInfo& info) {
  // ============================================================
  // 性能评分模型 - 针对学校选课/查成绩系统高并发场景优化
  // ============================================================
  // 权重配置：
  // - CPU 使用率: 35%
  // - 内存使用率: 30%
  // - CPU 负载:   15%
  // - 磁盘 IO:    15%
  // - 网络带宽:    5% (收发各 2.5%)
  // ============================================================
  
  const double cpu_weight = 0.35;
  const double mem_weight = 0.30;
  const double load_weight = 0.15;
  const double disk_weight = 0.15;
  const double net_weight = 0.05;

  const double load_coefficient = 1.5;  // I/O 密集型场景系数
  const double max_bandwidth = 125000000.0;  // 1Gbps

  double cpu_percent = 0, load_avg_1 = 0, mem_percent = 0;
  double net_recv_rate = 0, net_send_rate = 0, disk_util = 0;
  int cpu_cores = 1;

  if (info.cpu_stat_size() > 0) {
    cpu_percent = info.cpu_stat(0).cpu_percent();
    cpu_cores = info.cpu_stat_size() - 1;
    if (cpu_cores < 1) cpu_cores = 1;
  }
  if (info.has_cpu_load()) {
    load_avg_1 = info.cpu_load().load_avg_1();
  }
  if (info.has_mem_info()) {
    mem_percent = info.mem_info().used_percent();
  }
  if (info.net_info_size() > 0) {
    net_recv_rate = info.net_info(0).rcv_rate();
    net_send_rate = info.net_info(0).send_rate();
  }
  if (info.disk_info_size() > 0) {
    for (int i = 0; i < info.disk_info_size(); ++i) {
      double util = info.disk_info(i).util_percent();
      if (util > disk_util) disk_util = util;
    }
  }

  // 反向归一化
  auto clamp = [](double v) { return v < 0 ? 0 : (v > 1 ? 1 : v); };
  
  double cpu_score = clamp(1.0 - cpu_percent / 100.0);
  double mem_score = clamp(1.0 - mem_percent / 100.0);
  double load_score = clamp(1.0 - load_avg_1 / (cpu_cores * load_coefficient));
  double disk_score = clamp(1.0 - disk_util / 100.0);
  double net_recv_score = clamp(1.0 - net_recv_rate / max_bandwidth);
  double net_send_score = clamp(1.0 - net_send_rate / max_bandwidth);
  double net_score = (net_recv_score + net_send_score) / 2.0;

  double score = cpu_score * cpu_weight + mem_score * mem_weight +
                 load_score * load_weight + disk_score * disk_weight +
                 net_score * net_weight;

  score *= 100.0;
  return score < 0 ? 0 : (score > 100 ? 100 : score);
}

void HostManager::write_to_mysql(
    const std::string& host_name, const HostScore& host_score,
    double net_in_rate, double net_out_rate, float cpu_percent_rate,
    float usr_percent_rate, float system_percent_rate, float nice_percent_rate,
    float idle_percent_rate, float io_wait_percent_rate, float irq_percent_rate,
    float soft_irq_percent_rate, float steal_percent_rate,
    float guest_percent_rate, float guest_nice_percent_rate,
    float load_avg_1_rate, float load_avg_3_rate, float load_avg_15_rate,
    float mem_used_percent_rate, float mem_total_rate, float mem_free_rate,
    float mem_avail_rate, float net_in_rate_rate, float net_out_rate_rate,
    float net_in_drop_rate_rate, float net_out_drop_rate_rate) {
  MYSQL* conn = mysql_init(NULL);
  if (!conn) {
    fastlog::file::get_logger(kManagerLoggerName)->error("mysql_init failed");
    return;
  }
  if (!mysql_real_connect(conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS, MYSQL_DB, 0,
                          NULL, 0)) {
    auto* log = fastlog::file::get_logger(kManagerLoggerName);
    if (log) log->error("mysql_real_connect failed: {}", mysql_error(conn));
    // fastlog::console.error("mysql_real_connect failed: {}", mysql_error(conn));
    std::cerr << "[ERROR] MySQL connect failed: " << mysql_error(conn) << "\n";
    mysql_close(conn);
    return;
  }

  auto log_mysql_error = [conn]() {
    auto* log = fastlog::file::get_logger(kManagerLoggerName);
    if (log) log->error("MySQL query failed: {}", mysql_error(conn));
    // fastlog::console.error("MySQL query failed: {}", mysql_error(conn));
    std::cerr << "[ERROR] MySQL: " << mysql_error(conn) << "\n";
  };

  // 时间戳
  std::time_t t = std::chrono::system_clock::to_time_t(host_score.timestamp);
  std::tm tm_time;
  localtime_r(&t, &tm_time);
  char time_buf[32];
  std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_time);

  const auto& info = host_score.info;
  auto rate = [](float now_val, float last_val) -> float {
    if (last_val == 0) return 0;
    return (now_val - last_val) / last_val;
  };

  // ========== 1. 写入主表 server_performance ==========
  {
    float total = 0, free_mem = 0, avail = 0, send_rate = 0, rcv_rate = 0;
    float cpu_percent = 0, usr_percent = 0, system_percent = 0;
    float nice_percent = 0, idle_percent = 0, io_wait_percent = 0;
    float irq_percent = 0, soft_irq_percent = 0;
    float load_avg_1 = 0, load_avg_3 = 0, load_avg_15 = 0, mem_used_percent = 0;
    float disk_util_percent = 0;

    if (info.has_mem_info()) {
      total = info.mem_info().total();
      free_mem = info.mem_info().free();
      avail = info.mem_info().avail();
      mem_used_percent = info.mem_info().used_percent();
    }
    if (info.net_info_size() > 0) {
      send_rate = info.net_info(0).send_rate() / 1024.0;
      rcv_rate = info.net_info(0).rcv_rate() / 1024.0;
    }
    if (info.cpu_stat_size() > 0) {
      const auto& cpu = info.cpu_stat(0);
      cpu_percent = cpu.cpu_percent();
      usr_percent = cpu.usr_percent();
      system_percent = cpu.system_percent();
      nice_percent = cpu.nice_percent();
      idle_percent = cpu.idle_percent();
      io_wait_percent = cpu.io_wait_percent();
      irq_percent = cpu.irq_percent();
      soft_irq_percent = cpu.soft_irq_percent();
    }
    if (info.has_cpu_load()) {
      load_avg_1 = info.cpu_load().load_avg_1();
      load_avg_3 = info.cpu_load().load_avg_3();
      load_avg_15 = info.cpu_load().load_avg_15();
    }
    // 获取磁盘利用率最大值（用于评分）
    for (int i = 0; i < info.disk_info_size(); ++i) {
      float util = info.disk_info(i).util_percent();
      if (util > disk_util_percent) disk_util_percent = util;
    }

    // 计算磁盘利用率变化率
    static std::map<std::string, float> last_disk_util;
    float disk_util_percent_rate = 0;
    if (last_disk_util.count(host_name) && last_disk_util[host_name] != 0) {
      disk_util_percent_rate = (disk_util_percent - last_disk_util[host_name]) / last_disk_util[host_name];
    }
    last_disk_util[host_name] = disk_util_percent;

    std::ostringstream oss;
    oss << "INSERT INTO server_performance "
        << "(server_name, cpu_percent, usr_percent, system_percent, nice_percent, "
        << "idle_percent, io_wait_percent, irq_percent, soft_irq_percent, "
        << "load_avg_1, load_avg_3, load_avg_15, "
        << "mem_used_percent, total, free, avail, "
        << "disk_util_percent, send_rate, rcv_rate, score, "
        << "cpu_percent_rate, usr_percent_rate, system_percent_rate, "
        << "nice_percent_rate, idle_percent_rate, io_wait_percent_rate, "
        << "irq_percent_rate, soft_irq_percent_rate, "
        << "load_avg_1_rate, load_avg_3_rate, load_avg_15_rate, "
        << "mem_used_percent_rate, total_rate, free_rate, avail_rate, "
        << "disk_util_percent_rate, send_rate_rate, rcv_rate_rate, timestamp) VALUES ('"
        << host_name << "',"
        << cpu_percent << "," << usr_percent << "," << system_percent << ","
        << nice_percent << "," << idle_percent << "," << io_wait_percent << ","
        << irq_percent << "," << soft_irq_percent << ","
        << load_avg_1 << "," << load_avg_3 << "," << load_avg_15 << ","
        << mem_used_percent << "," << total << "," << free_mem << "," << avail << ","
        << disk_util_percent << "," << send_rate << "," << rcv_rate << "," << host_score.score << ","
        << cpu_percent_rate << "," << usr_percent_rate << "," << system_percent_rate << ","
        << nice_percent_rate << "," << idle_percent_rate << "," << io_wait_percent_rate << ","
        << irq_percent_rate << "," << soft_irq_percent_rate << ","
        << load_avg_1_rate << "," << load_avg_3_rate << "," << load_avg_15_rate << ","
        << mem_used_percent_rate << "," << mem_total_rate << "," << mem_free_rate << ","
        << mem_avail_rate << "," << disk_util_percent_rate << ","
        << net_in_rate_rate << "," << net_out_rate_rate
        << ",'" << time_buf << "')";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
      log_mysql_error();
    }
  }

  // ========== 2. 写入网络详细表 server_net_detail ==========
  for (int i = 0; i < info.net_info_size(); ++i) {
    const auto& net = info.net_info(i);
    std::string net_name = net.name();
    
    NetDetailSample curr;
    curr.rcv_bytes_rate = net.rcv_rate();
    curr.rcv_packets_rate = net.rcv_packets_rate();
    curr.snd_bytes_rate = net.send_rate();
    curr.snd_packets_rate = net.send_packets_rate();
    curr.err_in = net.err_in();
    curr.err_out = net.err_out();
    curr.drop_in = net.drop_in();
    curr.drop_out = net.drop_out();

    NetDetailSample& last = last_net_samples[host_name][net_name];
    
    // 计算错误/丢弃变化率
    auto rate_u64 = [](uint64_t now_val, uint64_t last_val) -> float {
      if (last_val == 0) return 0;
      return static_cast<float>(now_val - last_val) / static_cast<float>(last_val);
    };
    
    std::ostringstream oss;
    oss << "INSERT INTO server_net_detail "
        << "(server_name, net_name, err_in, err_out, drop_in, drop_out, "
        << "rcv_bytes_rate, rcv_packets_rate, snd_bytes_rate, snd_packets_rate, "
        << "rcv_bytes_rate_rate, rcv_packets_rate_rate, "
        << "snd_bytes_rate_rate, snd_packets_rate_rate, "
        << "err_in_rate, err_out_rate, drop_in_rate, drop_out_rate, "
        << "timestamp) VALUES ('"
        << host_name << "','" << net_name << "',"
        << curr.err_in << "," << curr.err_out << ","
        << curr.drop_in << "," << curr.drop_out << ","
        << curr.rcv_bytes_rate << "," << curr.rcv_packets_rate << ","
        << curr.snd_bytes_rate << "," << curr.snd_packets_rate << ","
        << rate(curr.rcv_bytes_rate, last.rcv_bytes_rate) << ","
        << rate(curr.rcv_packets_rate, last.rcv_packets_rate) << ","
        << rate(curr.snd_bytes_rate, last.snd_bytes_rate) << ","
        << rate(curr.snd_packets_rate, last.snd_packets_rate) << ","
        << rate_u64(curr.err_in, last.err_in) << ","
        << rate_u64(curr.err_out, last.err_out) << ","
        << rate_u64(curr.drop_in, last.drop_in) << ","
        << rate_u64(curr.drop_out, last.drop_out)
        << ",'" << time_buf << "')";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
      log_mysql_error();
    }
    last = curr;
  }

  // ========== 3. 写入软中断详细表 server_softirq_detail ==========
  for (int i = 0; i < info.soft_irq_size(); ++i) {
    const auto& sirq = info.soft_irq(i);
    std::string cpu_name = sirq.cpu();
    
    SoftIrqSample curr;
    curr.hi = sirq.hi();
    curr.timer = sirq.timer();
    curr.net_tx = sirq.net_tx();
    curr.net_rx = sirq.net_rx();
    curr.block = sirq.block();
    curr.irq_poll = sirq.irq_poll();
    curr.tasklet = sirq.tasklet();
    curr.sched = sirq.sched();
    curr.hrtimer = sirq.hrtimer();
    curr.rcu = sirq.rcu();

    SoftIrqSample& last = last_softirq_samples[host_name][cpu_name];
    
    std::ostringstream oss;
    oss << "INSERT INTO server_softirq_detail "
        << "(server_name, cpu_name, hi, timer, net_tx, net_rx, block, "
        << "irq_poll, tasklet, sched, hrtimer, rcu, "
        << "hi_rate, timer_rate, net_tx_rate, net_rx_rate, block_rate, "
        << "irq_poll_rate, tasklet_rate, sched_rate, hrtimer_rate, rcu_rate, "
        << "timestamp) VALUES ('"
        << host_name << "','" << cpu_name << "',"
        << curr.hi << "," << curr.timer << "," << curr.net_tx << ","
        << curr.net_rx << "," << curr.block << "," << curr.irq_poll << ","
        << curr.tasklet << "," << curr.sched << "," << curr.hrtimer << ","
        << curr.rcu << ","
        << rate(curr.hi, last.hi) << "," << rate(curr.timer, last.timer) << ","
        << rate(curr.net_tx, last.net_tx) << "," << rate(curr.net_rx, last.net_rx) << ","
        << rate(curr.block, last.block) << "," << rate(curr.irq_poll, last.irq_poll) << ","
        << rate(curr.tasklet, last.tasklet) << "," << rate(curr.sched, last.sched) << ","
        << rate(curr.hrtimer, last.hrtimer) << "," << rate(curr.rcu, last.rcu)
        << ",'" << time_buf << "')";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
      log_mysql_error();
    }
    last = curr;
  }

  // ========== 4. 写入内存详细表 server_mem_detail ==========
  if (info.has_mem_info()) {
    const auto& mem = info.mem_info();
    
    MemDetailSample curr;
    curr.total = mem.total();
    curr.free = mem.free();
    curr.avail = mem.avail();
    curr.buffers = mem.buffers();
    curr.cached = mem.cached();
    curr.swap_cached = mem.swap_cached();
    curr.active = mem.active();
    curr.inactive = mem.inactive();
    curr.active_anon = mem.active_anon();
    curr.inactive_anon = mem.inactive_anon();
    curr.active_file = mem.active_file();
    curr.inactive_file = mem.inactive_file();
    curr.dirty = mem.dirty();
    curr.writeback = mem.writeback();
    curr.anon_pages = mem.anon_pages();
    curr.mapped = mem.mapped();
    curr.kreclaimable = mem.kreclaimable();
    curr.sreclaimable = mem.sreclaimable();
    curr.sunreclaim = mem.sunreclaim();

    MemDetailSample& last = last_mem_samples[host_name];
    
    std::ostringstream oss;
    oss << "INSERT INTO server_mem_detail "
        << "(server_name, total, free, avail, buffers, cached, swap_cached, "
        << "active, inactive, active_anon, inactive_anon, active_file, inactive_file, "
        << "dirty, writeback, anon_pages, mapped, kreclaimable, sreclaimable, sunreclaim, "
        << "total_rate, free_rate, avail_rate, buffers_rate, cached_rate, swap_cached_rate, "
        << "active_rate, inactive_rate, active_anon_rate, inactive_anon_rate, "
        << "active_file_rate, inactive_file_rate, dirty_rate, writeback_rate, "
        << "anon_pages_rate, mapped_rate, kreclaimable_rate, sreclaimable_rate, "
        << "sunreclaim_rate, timestamp) VALUES ('"
        << host_name << "',"
        << curr.total << "," << curr.free << "," << curr.avail << ","
        << curr.buffers << "," << curr.cached << "," << curr.swap_cached << ","
        << curr.active << "," << curr.inactive << "," << curr.active_anon << ","
        << curr.inactive_anon << "," << curr.active_file << "," << curr.inactive_file << ","
        << curr.dirty << "," << curr.writeback << "," << curr.anon_pages << ","
        << curr.mapped << "," << curr.kreclaimable << "," << curr.sreclaimable << ","
        << curr.sunreclaim << ","
        << rate(curr.total, last.total) << "," << rate(curr.free, last.free) << ","
        << rate(curr.avail, last.avail) << "," << rate(curr.buffers, last.buffers) << ","
        << rate(curr.cached, last.cached) << "," << rate(curr.swap_cached, last.swap_cached) << ","
        << rate(curr.active, last.active) << "," << rate(curr.inactive, last.inactive) << ","
        << rate(curr.active_anon, last.active_anon) << "," << rate(curr.inactive_anon, last.inactive_anon) << ","
        << rate(curr.active_file, last.active_file) << "," << rate(curr.inactive_file, last.inactive_file) << ","
        << rate(curr.dirty, last.dirty) << "," << rate(curr.writeback, last.writeback) << ","
        << rate(curr.anon_pages, last.anon_pages) << "," << rate(curr.mapped, last.mapped) << ","
        << rate(curr.kreclaimable, last.kreclaimable) << "," << rate(curr.sreclaimable, last.sreclaimable) << ","
        << rate(curr.sunreclaim, last.sunreclaim)
        << ",'" << time_buf << "')";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
      log_mysql_error();
    }
    last = curr;
  }

  // ========== 5. 写入磁盘详细表 server_disk_detail ==========
  for (int i = 0; i < info.disk_info_size(); ++i) {
    const auto& disk = info.disk_info(i);
    std::string disk_name = disk.name();
    
    DiskDetailSample curr;
    curr.read_bytes_per_sec = disk.read_bytes_per_sec();
    curr.write_bytes_per_sec = disk.write_bytes_per_sec();
    curr.read_iops = disk.read_iops();
    curr.write_iops = disk.write_iops();
    curr.avg_read_latency_ms = disk.avg_read_latency_ms();
    curr.avg_write_latency_ms = disk.avg_write_latency_ms();
    curr.util_percent = disk.util_percent();

    DiskDetailSample& last = last_disk_samples[host_name][disk_name];
    
    std::ostringstream oss;
    oss << "INSERT INTO server_disk_detail "
        << "(server_name, disk_name, `reads`, `writes`, sectors_read, sectors_written, "
        << "read_time_ms, write_time_ms, io_in_progress, io_time_ms, weighted_io_time_ms, "
        << "read_bytes_per_sec, write_bytes_per_sec, read_iops, write_iops, "
        << "avg_read_latency_ms, avg_write_latency_ms, util_percent, "
        << "read_bytes_per_sec_rate, write_bytes_per_sec_rate, read_iops_rate, write_iops_rate, "
        << "avg_read_latency_ms_rate, avg_write_latency_ms_rate, util_percent_rate, "
        << "timestamp) VALUES ('"
        << host_name << "','" << disk_name << "',"
        << disk.reads() << "," << disk.writes() << ","
        << disk.sectors_read() << "," << disk.sectors_written() << ","
        << disk.read_time_ms() << "," << disk.write_time_ms() << ","
        << disk.io_in_progress() << "," << disk.io_time_ms() << ","
        << disk.weighted_io_time_ms() << ","
        << curr.read_bytes_per_sec << "," << curr.write_bytes_per_sec << ","
        << curr.read_iops << "," << curr.write_iops << ","
        << curr.avg_read_latency_ms << "," << curr.avg_write_latency_ms << ","
        << curr.util_percent << ","
        << rate(curr.read_bytes_per_sec, last.read_bytes_per_sec) << ","
        << rate(curr.write_bytes_per_sec, last.write_bytes_per_sec) << ","
        << rate(curr.read_iops, last.read_iops) << ","
        << rate(curr.write_iops, last.write_iops) << ","
        << rate(curr.avg_read_latency_ms, last.avg_read_latency_ms) << ","
        << rate(curr.avg_write_latency_ms, last.avg_write_latency_ms) << ","
        << rate(curr.util_percent, last.util_percent)
        << ",'" << time_buf << "')";
    if (mysql_query(conn, oss.str().c_str()) != 0) {
      log_mysql_error();
    }
    last = curr;
  }

  mysql_close(conn);

}

}  // namespace monitor
