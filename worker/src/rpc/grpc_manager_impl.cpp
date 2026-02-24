#include "rpc/grpc_manager_impl.hpp"
namespace monitor {
    ::grpc::Status GrpcManagerImpl::GetMonitorInfo(
    ::grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    ::monitor::proto::MonitorInfo* response) {
  // 实时采集监控数据
  _metric_collector->collect_all(response);
  return grpc::Status::OK;
}
}