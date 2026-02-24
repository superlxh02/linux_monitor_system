#pragma once

#include <grpcpp/support/status.h>

#include <memory>

#include "monitor/metric_collector.hpp"
#include "monitor_info.grpc.pb.h"
#include "monitor_info.pb.h"


namespace monitor{
    class GrpcManagerImpl : public monitor::proto::GrpcManager::Service{
        public:
            GrpcManagerImpl();
            ~GrpcManagerImpl() override = default;

            ::grpc::Status GetMonitorInfo(::grpc::ServerContext* context, const ::google::protobuf::Empty* request, ::monitor::proto::MonitorInfo* response) override;
        private:
            std::unique_ptr<MetricCollector> _metric_collector;
    };
}