#pragma once

#include <grpcpp/support/status.h>

#include <memory>

#include "monitor/metric_collector.hpp"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"



namespace monitor{
    //这是manager端的服务实现类，负责处理worker推送过来的监控数据。
    //注意不是worker端的server实现类
    class GrpcManagerImpl : public monitor::proto::GrpcManager::Service{
        public:
            GrpcManagerImpl();
            ~GrpcManagerImpl() override = default;

            ::grpc::Status GetMonitorInfo(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::monitor::proto::MonitorInfo* response) override;
        private:
            std::unique_ptr<MetricCollector> _metric_collector;
    };
}