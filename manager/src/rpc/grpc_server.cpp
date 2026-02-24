#include "rpc/grpc_server.hpp"

#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kManagerLoggerName[] = "manager_file_logger";
}

::grpc::Status GrpcServerImpl::SetMonitorInfo(
    ::grpc::ServerContext* context,
    const ::monitor::proto::MonitorInfo* request,
    ::google::protobuf::Empty* response) {
  if (!request) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Empty request");
  }

  std::string hostname = request->name();
  if (hostname.empty() && request->has_host_info()) {
    hostname = request->host_info().hostname();
  }

  if (hostname.empty()) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Missing hostname");
  }

  // 存储数据
  {
    std::lock_guard<std::mutex> lock(_mtx);
    _host_data[hostname] = {*request, std::chrono::system_clock::now()};
  }

  fastlog::file::get_logger(kManagerLoggerName)->debug("Received monitor data from: {}", hostname);
  // fastlog::console.debug("Received monitor data from: {}", hostname);

  // 调用回调函数
  if (_callback) {
    _callback(*request);
  }

  return grpc::Status::OK;
}

::grpc::Status GrpcServerImpl::GetMonitorInfo(
    ::grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    ::monitor::proto::MonitorInfo* response) {
  // 返回第一个主机的数据（或空）
  std::lock_guard<std::mutex> lock(_mtx);
  if (!_host_data.empty()) {
    *response = _host_data.begin()->second.info;
  }
  return grpc::Status::OK;
}

std::unordered_map<std::string, HostData> GrpcServerImpl::get_all_host_data() {
  std::lock_guard<std::mutex> lock(_mtx);
  return _host_data;
}

bool GrpcServerImpl::get_host_data(const std::string& hostname, HostData* data) {
  std::lock_guard<std::mutex> lock(_mtx);
  auto it = _host_data.find(hostname);
  if (it != _host_data.end()) {
    *data = it->second;
    return true;
  }
  return false;
}

}  // namespace monitor
