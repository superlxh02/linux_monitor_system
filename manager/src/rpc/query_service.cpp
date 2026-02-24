#include "rpc/query_service.hpp"

namespace monitor {

QueryServiceImpl::QueryServiceImpl(QueryManager* query_manager)
  : _query_manager(query_manager) {}

TimeRange QueryServiceImpl::convert_time_range(
    const ::monitor::proto::TimeRange& proto_range) {
  TimeRange range;
  range.start_time = std::chrono::system_clock::from_time_t(
      proto_range.start_time().seconds());
  range.end_time =
      std::chrono::system_clock::from_time_t(proto_range.end_time().seconds());
  return range;
}

void QueryServiceImpl::set_timestamp(
    ::google::protobuf::Timestamp* ts,
    const std::chrono::system_clock::time_point& tp) {
  auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                     tp.time_since_epoch())
                     .count();
  ts->set_seconds(seconds);
  ts->set_nanos(0);
}

::grpc::Status QueryServiceImpl::QueryPerformance(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryPerformanceRequest* request,
    ::monitor::proto::QueryPerformanceResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  // 验证时间范围
  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_performance(
      request->server_name(), time_range, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_cpu_percent(rec.cpu_percent);
    proto_rec->set_usr_percent(rec.usr_percent);
    proto_rec->set_system_percent(rec.system_percent);
    proto_rec->set_nice_percent(rec.nice_percent);
    proto_rec->set_idle_percent(rec.idle_percent);
    proto_rec->set_io_wait_percent(rec.io_wait_percent);
    proto_rec->set_irq_percent(rec.irq_percent);
    proto_rec->set_soft_irq_percent(rec.soft_irq_percent);
    proto_rec->set_load_avg_1(rec.load_avg_1);
    proto_rec->set_load_avg_3(rec.load_avg_3);
    proto_rec->set_load_avg_15(rec.load_avg_15);
    proto_rec->set_mem_used_percent(rec.mem_used_percent);
    proto_rec->set_mem_total(rec.mem_total);
    proto_rec->set_mem_free(rec.mem_free);
    proto_rec->set_mem_avail(rec.mem_avail);
    proto_rec->set_disk_util_percent(rec.disk_util_percent);
    proto_rec->set_send_rate(rec.send_rate);
    proto_rec->set_rcv_rate(rec.rcv_rate);
    proto_rec->set_score(rec.score);
    proto_rec->set_cpu_percent_rate(rec.cpu_percent_rate);
    proto_rec->set_mem_used_percent_rate(rec.mem_used_percent_rate);
    proto_rec->set_disk_util_percent_rate(rec.disk_util_percent_rate);
    proto_rec->set_load_avg_1_rate(rec.load_avg_1_rate);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryTrend(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryTrendRequest* request,
    ::monitor::proto::QueryTrendResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  auto records = _query_manager->query_trend(
      request->server_name(), time_range, request->interval_seconds());

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_cpu_percent(rec.cpu_percent);
    proto_rec->set_usr_percent(rec.usr_percent);
    proto_rec->set_system_percent(rec.system_percent);
    proto_rec->set_io_wait_percent(rec.io_wait_percent);
    proto_rec->set_load_avg_1(rec.load_avg_1);
    proto_rec->set_load_avg_3(rec.load_avg_3);
    proto_rec->set_load_avg_15(rec.load_avg_15);
    proto_rec->set_mem_used_percent(rec.mem_used_percent);
    proto_rec->set_disk_util_percent(rec.disk_util_percent);
    proto_rec->set_send_rate(rec.send_rate);
    proto_rec->set_rcv_rate(rec.rcv_rate);
    proto_rec->set_score(rec.score);
    proto_rec->set_cpu_percent_rate(rec.cpu_percent_rate);
    proto_rec->set_mem_used_percent_rate(rec.mem_used_percent_rate);
    proto_rec->set_disk_util_percent_rate(rec.disk_util_percent_rate);
    proto_rec->set_load_avg_1_rate(rec.load_avg_1_rate);
  }

  response->set_interval_seconds(request->interval_seconds());

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryAnomaly(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryAnomalyRequest* request,
    ::monitor::proto::QueryAnomalyResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  AnomalyThresholds thresholds;
  thresholds.cpu_threshold =
      request->cpu_threshold() > 0 ? request->cpu_threshold() : 80.0f;
  thresholds.mem_threshold =
      request->mem_threshold() > 0 ? request->mem_threshold() : 90.0f;
  thresholds.disk_threshold =
      request->disk_threshold() > 0 ? request->disk_threshold() : 85.0f;
  thresholds.change_rate_threshold =
      request->change_rate_threshold() > 0 ? request->change_rate_threshold()
                                           : 0.5f;

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_anomaly(
      request->server_name(), time_range, thresholds, page, page_size,
      &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_anomalies();
    proto_rec->set_server_name(rec.server_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_anomaly_type(rec.anomaly_type);
    proto_rec->set_severity(rec.severity);
    proto_rec->set_value(rec.value);
    proto_rec->set_threshold(rec.threshold);
    proto_rec->set_metric_name(rec.metric_name);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}


::grpc::Status QueryServiceImpl::QueryScoreRank(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryScoreRankRequest* request,
    ::monitor::proto::QueryScoreRankResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  SortOrder order = (request->order() == ::monitor::proto::ASC)
                        ? SortOrder::ASC
                        : SortOrder::DESC;

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records =
      _query_manager->query_score_rank(order, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_servers();
    proto_rec->set_server_name(rec.server_name);
    proto_rec->set_score(rec.score);
    set_timestamp(proto_rec->mutable_last_update(), rec.last_update);
    proto_rec->set_status(rec.status == ServerStatus::ONLINE
                              ? ::monitor::proto::ONLINE
                              : ::monitor::proto::OFFLINE);
    proto_rec->set_cpu_percent(rec.cpu_percent);
    proto_rec->set_mem_used_percent(rec.mem_used_percent);
    proto_rec->set_disk_util_percent(rec.disk_util_percent);
    proto_rec->set_load_avg_1(rec.load_avg_1);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryLatestScore(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryLatestScoreRequest* request,
    ::monitor::proto::QueryLatestScoreResponse* response) {
  (void)context;
  (void)request;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  ClusterStats stats;
  auto records = _query_manager->query_latest_score(&stats);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_servers();
    proto_rec->set_server_name(rec.server_name);
    proto_rec->set_score(rec.score);
    set_timestamp(proto_rec->mutable_last_update(), rec.last_update);
    proto_rec->set_status(rec.status == ServerStatus::ONLINE
                              ? ::monitor::proto::ONLINE
                              : ::monitor::proto::OFFLINE);
    proto_rec->set_cpu_percent(rec.cpu_percent);
    proto_rec->set_mem_used_percent(rec.mem_used_percent);
    proto_rec->set_disk_util_percent(rec.disk_util_percent);
    proto_rec->set_load_avg_1(rec.load_avg_1);
  }

  auto* proto_stats = response->mutable_cluster_stats();
  proto_stats->set_total_servers(stats.total_servers);
  proto_stats->set_online_servers(stats.online_servers);
  proto_stats->set_offline_servers(stats.offline_servers);
  proto_stats->set_avg_score(stats.avg_score);
  proto_stats->set_max_score(stats.max_score);
  proto_stats->set_min_score(stats.min_score);
  proto_stats->set_best_server(stats.best_server);
  proto_stats->set_worst_server(stats.worst_server);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryNetDetail(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryDetailRequest* request,
    ::monitor::proto::QueryNetDetailResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_net_detail(
      request->server_name(), time_range, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    proto_rec->set_net_name(rec.net_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_err_in(rec.err_in);
    proto_rec->set_err_out(rec.err_out);
    proto_rec->set_drop_in(rec.drop_in);
    proto_rec->set_drop_out(rec.drop_out);
    proto_rec->set_rcv_bytes_rate(rec.rcv_bytes_rate);
    proto_rec->set_snd_bytes_rate(rec.snd_bytes_rate);
    proto_rec->set_rcv_packets_rate(rec.rcv_packets_rate);
    proto_rec->set_snd_packets_rate(rec.snd_packets_rate);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryDiskDetail(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryDetailRequest* request,
    ::monitor::proto::QueryDiskDetailResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_disk_detail(
      request->server_name(), time_range, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    proto_rec->set_disk_name(rec.disk_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_read_bytes_per_sec(rec.read_bytes_per_sec);
    proto_rec->set_write_bytes_per_sec(rec.write_bytes_per_sec);
    proto_rec->set_read_iops(rec.read_iops);
    proto_rec->set_write_iops(rec.write_iops);
    proto_rec->set_avg_read_latency_ms(rec.avg_read_latency_ms);
    proto_rec->set_avg_write_latency_ms(rec.avg_write_latency_ms);
    proto_rec->set_util_percent(rec.util_percent);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QueryMemDetail(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryDetailRequest* request,
    ::monitor::proto::QueryMemDetailResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_mem_detail(
      request->server_name(), time_range, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_total(rec.total);
    proto_rec->set_free(rec.free);
    proto_rec->set_avail(rec.avail);
    proto_rec->set_buffers(rec.buffers);
    proto_rec->set_cached(rec.cached);
    proto_rec->set_active(rec.active);
    proto_rec->set_inactive(rec.inactive);
    proto_rec->set_dirty(rec.dirty);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

::grpc::Status QueryServiceImpl::QuerySoftIrqDetail(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryDetailRequest* request,
    ::monitor::proto::QuerySoftIrqDetailResponse* response) {
  (void)context;

  if (!_query_manager) {
    return grpc::Status(grpc::StatusCode::UNAVAILABLE,
                        "Query manager not initialized");
  }

  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range)) {
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                        "Invalid time range: start_time > end_time");
  }

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 100;

  int total_count = 0;
  auto records = _query_manager->query_softirq_detail(
      request->server_name(), time_range, page, page_size, &total_count);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    proto_rec->set_cpu_name(rec.cpu_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_hi(rec.hi);
    proto_rec->set_timer(rec.timer);
    proto_rec->set_net_tx(rec.net_tx);
    proto_rec->set_net_rx(rec.net_rx);
    proto_rec->set_block(rec.block);
    proto_rec->set_sched(rec.sched);
  }

  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);

  return grpc::Status::OK;
}

}  // namespace monitor
