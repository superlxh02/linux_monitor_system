#pragma once

#include <grpcpp/grpcpp.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "monitor/metric_collector.hpp"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"


namespace monitor {

/**
 * 监控数据推送器
 * 
 * 每隔指定间隔（默认 10 秒）采集本机监控数据，
 * 并通过 gRPC 推送给管理者服务器。
 */
class MonitorPusher {
 public:

  explicit MonitorPusher(const std::string& manager_address,
                         int interval_seconds = 10);
  ~MonitorPusher();

  // 启动推送线程
  void start();

  // 停止推送
  void stop();

  // 获取管理者地址
  const std::string& get_manager_address() const { return _manager_address; }

 private:
  void push_for_loop();
  bool push_once();

  std::string _manager_address;  
  int _interval_seconds;
  std::atomic<bool> _running;
  std::unique_ptr<std::thread> _thread;
  std::unique_ptr<MetricCollector> _collector;
  std::unique_ptr<monitor::proto::GrpcManager::Stub> _stub;
};

}  // namespace monitor
