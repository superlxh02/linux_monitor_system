#pragma once

#include <grpc/grpc.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>

#include <string>

#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"

namespace monitor {

// RPC 客户端 - 用于从远程主机获取监控数据
class RpcClient {
 public:
  explicit RpcClient(const std::string& host_address = "localhost:50051");
  ~RpcClient();

  // SetMonitorInfo 已移除 - Server 端现在本地采集数据

  // 从远程主机获取监控数据
  bool get_monitor_info(monitor::proto::MonitorInfo* monitor_info);

  // 获取连接的主机地址
  const std::string& get_host_address() const { return _host_address; }

 private:
  std::unique_ptr<monitor::proto::GrpcManager::Stub> _stub_ptr;
  std::string _host_address;
};

}  // namespace monitor
