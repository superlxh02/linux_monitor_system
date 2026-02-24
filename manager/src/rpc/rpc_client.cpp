#include "rpc/rpc_client.hpp"

#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kManagerLoggerName[] = "manager_file_logger";
}

RpcClient::RpcClient(const std::string& host_address)
    : _host_address(host_address) {
  auto channel =
      grpc::CreateChannel(host_address, grpc::InsecureChannelCredentials());
  _stub_ptr = monitor::proto::GrpcManager::NewStub(channel);
}

RpcClient::~RpcClient() {}

// SetMonitorInfo 已移除 - Server 端现在本地采集数据

bool RpcClient::get_monitor_info(monitor::proto::MonitorInfo* monitor_info) {
  if (!monitor_info) {
    return false;
  }

  ::grpc::ClientContext context;
  ::google::protobuf::Empty request;

  ::grpc::Status status =
      _stub_ptr->GetMonitorInfo(&context, request, monitor_info);

  if (status.ok()) {
    return true;
  } else {
    fastlog::file::get_logger(kManagerLoggerName)->error("Failed to get monitor info from {}: {}",
                           _host_address, status.error_message());
    // fastlog::console.error("Failed to get monitor info from {}: {}", _host_address, status.error_message());
    return false;
  }
}

}  // namespace monitor
