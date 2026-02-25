#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "scoring_profile.hpp"
#include "monitor_info.pb.h"

namespace monitor {

struct HostScore {
  monitor::proto::MonitorInfo info;
  double score;
  std::chrono::system_clock::time_point timestamp;
};

// 管理多个远程主机的监控数据（推送模式）
class HostManager {
 public:
  HostManager();
  ~HostManager();

  // 启动后台处理线程
  void start();
  void stop();

  // 接收工作者推送的数据（由 gRPC 服务调用）
  void on_data_received(const monitor::proto::MonitorInfo& info);

  // 获取所有主机评分
  std::unordered_map<std::string, HostScore> get_all_host_scores();

  // 获取最优主机
  std::string get_best_host();

 private:
  void process_for_loop();
  double calc_scores(const monitor::proto::MonitorInfo& info,
                     ScoringProfile profile = ScoringProfile::BALANCED);
  void write_to_mysql(const std::string& host_name, const HostScore& host_score,
                    double net_in_rate, double net_out_rate,
                    float cpu_percent_rate, float usr_percent_rate,
                    float system_percent_rate, float nice_percent_rate,
                    float idle_percent_rate, float io_wait_percent_rate,
                    float irq_percent_rate, float soft_irq_percent_rate,
                    float steal_percent_rate, float guest_percent_rate,
                    float guest_nice_percent_rate, float load_avg_1_rate,
                    float load_avg_3_rate, float load_avg_15_rate,
                    float mem_used_percent_rate, float mem_total_rate,
                    float mem_free_rate, float mem_avail_rate,
                    float net_in_rate_rate, float net_out_rate_rate,
                    float net_in_drop_rate_rate, float net_out_drop_rate_rate);

  std::unordered_map<std::string, HostScore> _host_scores;
  std::mutex _mtx;
  std::atomic<bool> _running;
  std::unique_ptr<std::thread> _thread;
};

}  // namespace monitor
