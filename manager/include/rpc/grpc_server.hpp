#pragma once

#include <grpcpp/support/status.h>
#include <grpcpp/server_context.h>

#include <functional>
#include <mutex>
#include <unordered_map>
#include <chrono>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {

struct HostData {
  monitor::proto::MonitorInfo info;
  std::chrono::system_clock::time_point timestamp;
};

// 数据接收回调函数类型
using data_received_callback_t =
  std::function<void(const monitor::proto::MonitorInfo&)>;

// gRPC 服务实现类 - 接收工作者推送的监控数据
class GrpcServerImpl : public monitor::proto::GrpcManager::Service {
 public:
  GrpcServerImpl() = default;
  virtual ~GrpcServerImpl() = default;

  // 接收工作者推送的监控数据
  ::grpc::Status SetMonitorInfo(::grpc::ServerContext* context,
                                const ::monitor::proto::MonitorInfo* request,
                                ::google::protobuf::Empty* response) override;

  // 获取监控数据（保留接口）
  ::grpc::Status GetMonitorInfo(::grpc::ServerContext* context,
                                const ::google::protobuf::Empty* request,
                                ::monitor::proto::MonitorInfo* response) override;

  // 设置数据接收回调
  void set_data_received_callback(data_received_callback_t callback) {
    _callback = std::move(callback);
  }

  // 获取所有主机数据
  std::unordered_map<std::string, HostData> get_all_host_data();

  // 获取指定主机数据
  bool get_host_data(const std::string& hostname, HostData* data);

 private:
  std::mutex _mtx;
  std::unordered_map<std::string, HostData> _host_data;
  data_received_callback_t _callback;
};

}  // namespace monitor
