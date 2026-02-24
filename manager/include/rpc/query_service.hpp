#pragma once

#include <grpcpp/support/status.h>
#include <grpcpp/server_context.h>

#include "query_api.grpc.pb.h"
#include "query_api.pb.h"
#include "../query_manager.hpp"

namespace monitor {

// gRPC 查询服务实现类
class QueryServiceImpl : public monitor::proto::QueryService::Service {
 public:
  explicit QueryServiceImpl(QueryManager* query_manager);
  virtual ~QueryServiceImpl() = default;

  // 时间段性能数据查询
  ::grpc::Status QueryPerformance(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryPerformanceRequest* request,
      ::monitor::proto::QueryPerformanceResponse* response) override;

  // 变化率趋势查询
  ::grpc::Status QueryTrend(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryTrendRequest* request,
      ::monitor::proto::QueryTrendResponse* response) override;

  // 异常数据查询
  ::grpc::Status QueryAnomaly(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryAnomalyRequest* request,
      ::monitor::proto::QueryAnomalyResponse* response) override;

  // 评分排序查询
  ::grpc::Status QueryScoreRank(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryScoreRankRequest* request,
      ::monitor::proto::QueryScoreRankResponse* response) override;

  // 最新评分查询
  ::grpc::Status QueryLatestScore(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryLatestScoreRequest* request,
      ::monitor::proto::QueryLatestScoreResponse* response) override;

  // 网络详细数据查询
  ::grpc::Status QueryNetDetail(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryDetailRequest* request,
      ::monitor::proto::QueryNetDetailResponse* response) override;

  // 磁盘详细数据查询
  ::grpc::Status QueryDiskDetail(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryDetailRequest* request,
      ::monitor::proto::QueryDiskDetailResponse* response) override;

  // 内存详细数据查询
  ::grpc::Status QueryMemDetail(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryDetailRequest* request,
      ::monitor::proto::QueryMemDetailResponse* response) override;

  // 软中断详细数据查询
  ::grpc::Status QuerySoftIrqDetail(
      ::grpc::ServerContext* context,
      const ::monitor::proto::QueryDetailRequest* request,
      ::monitor::proto::QuerySoftIrqDetailResponse* response) override;

 private:
  // 转换时间范围
    TimeRange convert_time_range(const ::monitor::proto::TimeRange& proto_range);

  // 转换时间点到protobuf Timestamp
    void set_timestamp(::google::protobuf::Timestamp* ts,
                                         const std::chrono::system_clock::time_point& tp);

    QueryManager* _query_manager;
};

}  // namespace monitor
