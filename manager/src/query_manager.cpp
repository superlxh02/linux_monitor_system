#include "query_manager.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kManagerLoggerName[] = "manager_file_logger";

struct ScoreWeights {
  double cpu_weight;
  double mem_weight;
  double load_weight;
  double disk_weight;
  double net_weight;
  double load_coefficient;
  double max_bandwidth;
};

ScoreWeights get_score_weights(ScoringProfile profile) {
  switch (profile) {
    case ScoringProfile::HIGH_CONCURRENCY:
      return ScoreWeights{0.45, 0.25, 0.15, 0.10, 0.05, 1.2, 125000000.0};
    case ScoringProfile::IO_INTENSIVE:
      return ScoreWeights{0.20, 0.15, 0.20, 0.35, 0.10, 2.0, 125000000.0};
    case ScoringProfile::MEMORY_SENSITIVE:
      return ScoreWeights{0.20, 0.45, 0.15, 0.10, 0.10, 1.5, 125000000.0};
    case ScoringProfile::BALANCED:
    default:
      return ScoreWeights{0.35, 0.30, 0.15, 0.15, 0.05, 1.5, 125000000.0};
  }
}

double calc_score_by_profile(float cpu_percent, float mem_percent,
                             float load_avg_1, float disk_util_percent,
                             float send_rate_kb, float rcv_rate_kb,
                             ScoringProfile profile, int cpu_cores = 4) {
  const ScoreWeights w = get_score_weights(profile);
  const auto clamp01 = [](double v) {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
  };

  if (cpu_cores < 1) {
    cpu_cores = 1;
  }

  const double net_send_bytes = static_cast<double>(send_rate_kb) * 1024.0;
  const double net_rcv_bytes = static_cast<double>(rcv_rate_kb) * 1024.0;

  const double cpu_score = clamp01(1.0 - cpu_percent / 100.0);
  const double mem_score = clamp01(1.0 - mem_percent / 100.0);
  const double load_score =
      clamp01(1.0 - load_avg_1 / (cpu_cores * w.load_coefficient));
  const double disk_score = clamp01(1.0 - disk_util_percent / 100.0);
  const double net_recv_score = clamp01(1.0 - net_rcv_bytes / w.max_bandwidth);
  const double net_send_score = clamp01(1.0 - net_send_bytes / w.max_bandwidth);
  const double net_score = (net_recv_score + net_send_score) / 2.0;

  const double score = cpu_score * w.cpu_weight + mem_score * w.mem_weight +
                       load_score * w.load_weight + disk_score * w.disk_weight +
                       net_score * w.net_weight;
  return clamp01(score) * 100.0;
}
}

QueryManager::QueryManager() = default;

QueryManager::~QueryManager() { close(); }

bool QueryManager::init(const std::string &host, const std::string &user,
                        const std::string &password,
                        const std::string &database) {
  std::lock_guard<std::mutex> lock(_mtx);
  if (_initialized) {
    return true;
  }

  conn_ = mysql_init(nullptr);
  if (!conn_) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: mysql_init failed");
    return false;
  }

  if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                          database.c_str(), 0, nullptr, 0)) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: mysql_real_connect failed: {}",
                 mysql_error(conn_));
    mysql_close(conn_);
    conn_ = nullptr;
    return false;
  }

  // 设置字符集
  mysql_set_character_set(conn_, "utf8mb4");
  _initialized = true;
  fastlog::file::get_logger(kManagerLoggerName)->info("QueryManager: MySQL connection initialized");
  return true;

  fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: MySQL support not enabled");
  return false;
}

void QueryManager::close() {
  std::lock_guard<std::mutex> lock(_mtx);
  if (conn_) {
    mysql_close(conn_);
    conn_ = nullptr;
  }
  _initialized = false;
}

bool QueryManager::validate_timerange(const TimeRange &range) const {
  return range.start_time <= range.end_time;
}

std::string QueryManager::format_time(
    const std::chrono::system_clock::time_point &tp) const {
  std::time_t t = std::chrono::system_clock::to_time_t(tp);
  std::tm tm_time;
  localtime_r(&t, &tm_time);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_time);
  return std::string(buf);
}

std::chrono::system_clock::time_point
QueryManager::parse_time(const char *str) const {
  std::tm tm = {};
  std::istringstream ss(str);
  ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

int QueryManager::get_total_count(const std::string &count_sql) {
  if (mysql_query(conn_, count_sql.c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: count query failed: {}",
                           mysql_error(conn_));
    return 0;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return 0;
  }

  int count = 0;
  MYSQL_ROW row = mysql_fetch_row(result);
  if (row && row[0]) {
    count = std::atoi(row[0]);
  }
  mysql_free_result(result);
  return count;
}

std::vector<PerformanceRecord>
QueryManager::query_performance(const std::string &server_name,
                                const TimeRange &time_range, int page,
                                int page_size, int *total_count,
                                ScoringProfile scoring_profile) {
  std::vector<PerformanceRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  // 验证参数
  if (!validate_timerange(time_range)) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: Invalid time range");
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_performance WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
         "system_percent, nice_percent, idle_percent, io_wait_percent, "
         "irq_percent, soft_irq_percent, load_avg_1, load_avg_3, load_avg_15, "
         "mem_used_percent, total, free, avail, disk_util_percent, "
         "send_rate, rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
         "disk_util_percent_rate, load_avg_1_rate, send_rate_rate, "
         "rcv_rate_rate "
         "FROM server_performance WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: query failed: {}", mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    PerformanceRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.system_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.nice_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.idle_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.irq_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.soft_irq_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_3 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_total = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_free = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_avail = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.score = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate_rate = row[i] ? std::atof(row[i]) : 0;
    rec.score = static_cast<float>(calc_score_by_profile(
      rec.cpu_percent, rec.mem_used_percent, rec.load_avg_1,
      rec.disk_util_percent, rec.send_rate, rec.rcv_rate, scoring_profile));
    records.push_back(rec);
  }
  mysql_free_result(result);
  return records;
}

std::vector<PerformanceRecord>
QueryManager::query_trend(const std::string &server_name,
                          const TimeRange &time_range, int interval_seconds,
                          ScoringProfile scoring_profile) {
  std::vector<PerformanceRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: Invalid time range");
    return records;
  }

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  std::ostringstream sql;
  if (interval_seconds > 0) {
    // 带聚合的查询
    sql << "SELECT server_name, "
           "FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(timestamp) / "
        << interval_seconds << ") * " << interval_seconds
        << ") as time_bucket, "
           "AVG(cpu_percent) as cpu_percent, "
           "AVG(usr_percent) as usr_percent, "
           "AVG(system_percent) as system_percent, "
           "AVG(io_wait_percent) as io_wait_percent, "
           "AVG(load_avg_1) as load_avg_1, "
           "AVG(load_avg_3) as load_avg_3, "
           "AVG(load_avg_15) as load_avg_15, "
           "AVG(mem_used_percent) as mem_used_percent, "
           "AVG(disk_util_percent) as disk_util_percent, "
           "AVG(send_rate) as send_rate, "
           "AVG(rcv_rate) as rcv_rate, "
           "AVG(score) as score, "
           "AVG(cpu_percent_rate) as cpu_percent_rate, "
           "AVG(mem_used_percent_rate) as mem_used_percent_rate, "
           "AVG(disk_util_percent_rate) as disk_util_percent_rate, "
           "AVG(load_avg_1_rate) as load_avg_1_rate "
           "FROM server_performance WHERE server_name='"
        << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
        << end_time
        << "' GROUP BY server_name, time_bucket ORDER BY time_bucket";
  } else {
    // 不聚合，直接查询
    sql << "SELECT server_name, timestamp, cpu_percent, usr_percent, "
           "system_percent, io_wait_percent, load_avg_1, load_avg_3, "
           "load_avg_15, mem_used_percent, disk_util_percent, send_rate, "
           "rcv_rate, score, cpu_percent_rate, mem_used_percent_rate, "
           "disk_util_percent_rate, load_avg_1_rate "
           "FROM server_performance WHERE server_name='"
        << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
        << end_time << "' ORDER BY timestamp";
  }

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: trend query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    PerformanceRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.system_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_3 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_15 = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.send_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.score = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cpu_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.mem_used_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.disk_util_percent_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.load_avg_1_rate = row[i] ? std::atof(row[i]) : 0;
    rec.score = static_cast<float>(calc_score_by_profile(
      rec.cpu_percent, rec.mem_used_percent, rec.load_avg_1,
      rec.disk_util_percent, rec.send_rate, rec.rcv_rate, scoring_profile));
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

std::vector<AnomalyRecord>
QueryManager::query_anomaly(const std::string &server_name,
                            const TimeRange &time_range,
                            const AnomalyThresholds &thresholds, int page,
                            int page_size, int *total_count) {
  std::vector<AnomalyRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: Invalid time range");
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 构建WHERE条件
  std::ostringstream where_clause;
  where_clause << "timestamp BETWEEN '" << start_time << "' AND '" << end_time
               << "'";
  if (!server_name.empty()) {
    where_clause << " AND server_name='" << server_name << "'";
  }
  where_clause << " AND (cpu_percent > " << thresholds.cpu_threshold
               << " OR mem_used_percent > " << thresholds.mem_threshold
               << " OR disk_util_percent > " << thresholds.disk_threshold
               << " OR ABS(cpu_percent_rate) > "
               << thresholds.change_rate_threshold
               << " OR ABS(mem_used_percent_rate) > "
               << thresholds.change_rate_threshold << ")";

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_performance WHERE "
            << where_clause.str();
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, cpu_percent, mem_used_percent, "
         "disk_util_percent, cpu_percent_rate, mem_used_percent_rate "
         "FROM server_performance WHERE "
      << where_clause.str() << " ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: anomaly query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    std::string srv_name = row[0] ? row[0] : "";
    auto ts = row[1] ? parse_time(row[1]) : std::chrono::system_clock::now();
    float cpu = row[2] ? std::atof(row[2]) : 0;
    float mem = row[3] ? std::atof(row[3]) : 0;
    float disk = row[4] ? std::atof(row[4]) : 0;
    float cpu_rate = row[5] ? std::atof(row[5]) : 0;
    float mem_rate = row[6] ? std::atof(row[6]) : 0;

    // 生成异常记录
    auto add_anomaly = [&](const std::string &type, const std::string &metric,
                           float value, float threshold) {
      AnomalyRecord rec;
      rec.server_name = srv_name;
      rec.timestamp = ts;
      rec.anomaly_type = type;
      rec.metric_name = metric;
      rec.value = value;
      rec.threshold = threshold;
      // 判断严重程度
      if (type == "CPU_HIGH" && value > 95) {
        rec.severity = "CRITICAL";
      } else if (type == "MEM_HIGH" && value > 95) {
        rec.severity = "CRITICAL";
      } else if (type == "DISK_HIGH" && value > 95) {
        rec.severity = "CRITICAL";
      } else if (type == "RATE_SPIKE" && std::abs(value) > 1.0) {
        rec.severity = "CRITICAL";
      } else {
        rec.severity = "WARNING";
      }
      records.push_back(rec);
    };

    if (cpu > thresholds.cpu_threshold) {
      add_anomaly("CPU_HIGH", "cpu_percent", cpu, thresholds.cpu_threshold);
    }
    if (mem > thresholds.mem_threshold) {
      add_anomaly("MEM_HIGH", "mem_used_percent", mem,
                  thresholds.mem_threshold);
    }
    if (disk > thresholds.disk_threshold) {
      add_anomaly("DISK_HIGH", "disk_util_percent", disk,
                  thresholds.disk_threshold);
    }
    if (std::abs(cpu_rate) > thresholds.change_rate_threshold) {
      add_anomaly("RATE_SPIKE", "cpu_percent_rate", cpu_rate,
                  thresholds.change_rate_threshold);
    }
    if (std::abs(mem_rate) > thresholds.change_rate_threshold) {
      add_anomaly("RATE_SPIKE", "mem_used_percent_rate", mem_rate,
                  thresholds.change_rate_threshold);
    }
  }
  mysql_free_result(result);

  return records;
}

std::vector<ServerScoreSummary>
QueryManager::query_score_rank(SortOrder order, int page, int page_size,
                               int *total_count,
                               ScoringProfile scoring_profile) {
  std::vector<ServerScoreSummary> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  // 获取总数（不同服务器数量）
  std::string count_sql =
      "SELECT COUNT(DISTINCT server_name) FROM server_performance";
  if (total_count) {
    *total_count = get_total_count(count_sql);
  }

  // 查询每台服务器的最新数据，评分按所选 profile 在内存中计算与排序
  int offset = (page - 1) * page_size;

  std::ostringstream sql;
    sql << "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
      "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1, "
      "p1.send_rate, p1.rcv_rate "
         "FROM server_performance p1 "
         "INNER JOIN ("
         "  SELECT server_name, MAX(timestamp) as max_ts "
         "  FROM server_performance GROUP BY server_name"
        ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = p2.max_ts";

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: score rank query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  auto now = std::chrono::system_clock::now();
  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    ServerScoreSummary rec;
    rec.server_name = row[0] ? row[0] : "";
    rec.score = row[1] ? std::atof(row[1]) : 0;
    rec.last_update = row[2] ? parse_time(row[2]) : now;
    rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
    rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
    rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
    rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;
    float send_rate = row[7] ? std::atof(row[7]) : 0;
    float rcv_rate = row[8] ? std::atof(row[8]) : 0;
    rec.score = static_cast<float>(calc_score_by_profile(
        rec.cpu_percent, rec.mem_used_percent, rec.load_avg_1,
      rec.disk_util_percent, send_rate, rcv_rate, scoring_profile));

    // 判断在线状态（60秒阈值）
    auto age =
        std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_update)
            .count();
    rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

    records.push_back(rec);
  }
  mysql_free_result(result);

  std::sort(records.begin(), records.end(), [order](const auto &a, const auto &b) {
    if (order == SortOrder::ASC) {
      return a.score < b.score;
    }
    return a.score > b.score;
  });

  if (offset >= static_cast<int>(records.size())) {
    return {};
  }
  int end = std::min(offset + page_size, static_cast<int>(records.size()));
  return std::vector<ServerScoreSummary>(records.begin() + offset,
                                         records.begin() + end);
}

std::vector<ServerScoreSummary>
QueryManager::query_latest_score(ClusterStats *stats,
                                 ScoringProfile scoring_profile) {
  std::vector<ServerScoreSummary> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  // 查询每台服务器的最新数据
  std::string sql =
      "SELECT p1.server_name, p1.score, p1.timestamp, p1.cpu_percent, "
      "p1.mem_used_percent, p1.disk_util_percent, p1.load_avg_1, "
      "p1.send_rate, p1.rcv_rate "
      "FROM server_performance p1 "
      "INNER JOIN ("
      "  SELECT server_name, MAX(timestamp) as max_ts "
      "  FROM server_performance GROUP BY server_name"
      ") p2 ON p1.server_name = p2.server_name AND p1.timestamp = p2.max_ts "
      "ORDER BY p1.timestamp DESC";

  if (mysql_query(conn_, sql.c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: latest score query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  auto now = std::chrono::system_clock::now();
  float total_score = 0;
  float max_score = -1;
  float min_score = 101;
  std::string best_server, worst_server;
  int online_count = 0, offline_count = 0;

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    ServerScoreSummary rec;
    rec.server_name = row[0] ? row[0] : "";
    rec.score = row[1] ? std::atof(row[1]) : 0;
    rec.last_update = row[2] ? parse_time(row[2]) : now;
    rec.cpu_percent = row[3] ? std::atof(row[3]) : 0;
    rec.mem_used_percent = row[4] ? std::atof(row[4]) : 0;
    rec.disk_util_percent = row[5] ? std::atof(row[5]) : 0;
    rec.load_avg_1 = row[6] ? std::atof(row[6]) : 0;
    float send_rate = row[7] ? std::atof(row[7]) : 0;
    float rcv_rate = row[8] ? std::atof(row[8]) : 0;
    rec.score = static_cast<float>(calc_score_by_profile(
      rec.cpu_percent, rec.mem_used_percent, rec.load_avg_1,
      rec.disk_util_percent, send_rate, rcv_rate, scoring_profile));

    // 判断在线状态（60秒阈值）
    auto age =
        std::chrono::duration_cast<std::chrono::seconds>(now - rec.last_update)
            .count();
    rec.status = (age > 60) ? ServerStatus::OFFLINE : ServerStatus::ONLINE;

    if (rec.status == ServerStatus::ONLINE) {
      online_count++;
    } else {
      offline_count++;
    }

    // 统计
    total_score += rec.score;
    if (rec.score > max_score) {
      max_score = rec.score;
      best_server = rec.server_name;
    }
    if (rec.score < min_score) {
      min_score = rec.score;
      worst_server = rec.server_name;
    }

    records.push_back(rec);
  }
  mysql_free_result(result);

  std::sort(records.begin(), records.end(),
            [](const auto &a, const auto &b) { return a.score > b.score; });

  // 填充集群统计
  if (stats) {
    stats->total_servers = static_cast<int>(records.size());
    stats->online_servers = online_count;
    stats->offline_servers = offline_count;
    stats->avg_score = records.empty() ? 0 : total_score / records.size();
    stats->max_score = max_score > 0 ? max_score : 0;
    stats->min_score = min_score < 101 ? min_score : 0;
    stats->best_server = best_server;
    stats->worst_server = worst_server;
  }

  return records;
}

std::vector<NetDetailRecord>
QueryManager::query_net_detail(const std::string &server_name,
                               const TimeRange &time_range, int page,
                               int page_size, int *total_count) {
  std::vector<NetDetailRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_net_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, net_name, timestamp, err_in, err_out, "
         "drop_in, drop_out, rcv_bytes_rate, snd_bytes_rate, "
         "rcv_packets_rate, snd_packets_rate "
         "FROM server_net_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: net detail query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    NetDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.net_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.err_in = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.err_out = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.drop_in = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.drop_out = row[i] ? std::stoull(row[i]) : 0;
    i++;
    rec.rcv_bytes_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.snd_bytes_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.rcv_packets_rate = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.snd_packets_rate = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

std::vector<DiskDetailRecord>
QueryManager::query_disk_detail(const std::string &server_name,
                                const TimeRange &time_range, int page,
                                int page_size, int *total_count) {
  std::vector<DiskDetailRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_disk_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, disk_name, timestamp, read_bytes_per_sec, "
         "write_bytes_per_sec, read_iops, write_iops, avg_read_latency_ms, "
         "avg_write_latency_ms, util_percent "
         "FROM server_disk_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: disk detail query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    DiskDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.disk_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.read_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.write_bytes_per_sec = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.read_iops = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.write_iops = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avg_read_latency_ms = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avg_write_latency_ms = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.util_percent = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

std::vector<MemDetailRecord>
QueryManager::query_mem_detail(const std::string &server_name,
                               const TimeRange &time_range, int page,
                               int page_size, int *total_count) {
  std::vector<MemDetailRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_mem_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, timestamp, total, free, avail, buffers, "
         "cached, active, inactive, dirty "
         "FROM server_mem_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: mem detail query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    MemDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.total = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.free = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.avail = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.buffers = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.cached = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.active = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.inactive = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.dirty = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

std::vector<SoftIrqDetailRecord>
QueryManager::query_softirq_detail(const std::string &server_name,
                                   const TimeRange &time_range, int page,
                                   int page_size, int *total_count) {
  std::vector<SoftIrqDetailRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

  // 获取总数
  std::ostringstream count_sql;
  count_sql << "SELECT COUNT(*) FROM server_softirq_detail WHERE server_name='"
            << server_name << "' AND timestamp BETWEEN '" << start_time
            << "' AND '" << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  // 查询数据
  int offset = (page - 1) * page_size;
  std::ostringstream sql;
  sql << "SELECT server_name, cpu_name, timestamp, hi, timer, net_tx, "
         "net_rx, block, sched "
         "FROM server_softirq_detail WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time << "' ORDER BY timestamp DESC LIMIT " << page_size
      << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)->error("QueryManager: softirq detail query failed: {}",
                           mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    SoftIrqDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.cpu_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.hi = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.timer = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.net_tx = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.net_rx = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.block = row[i] ? std::stoll(row[i]) : 0;
    i++;
    rec.sched = row[i] ? std::stoll(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

std::vector<CpuCoreDetailRecord>
QueryManager::query_cpu_core_detail(const std::string &server_name,
                                    const TimeRange &time_range, int page,
                                    int page_size, int *total_count) {
  std::vector<CpuCoreDetailRecord> records;

  std::lock_guard<std::mutex> lock(_mtx);
  if (!_initialized || !conn_) {
    return records;
  }

  if (!validate_timerange(time_range)) {
    return records;
  }
  if (page < 1)
    page = 1;
  if (page_size < 1)
    page_size = 100;

  std::string start_time = format_time(time_range.start_time);
  std::string end_time = format_time(time_range.end_time);

    std::ostringstream count_sql;
    count_sql << "SELECT COUNT(DISTINCT cpu_name) FROM server_cpu_core_detail "
          << "WHERE server_name='" << server_name
          << "' AND timestamp BETWEEN '" << start_time << "' AND '"
          << end_time << "'";
  if (total_count) {
    *total_count = get_total_count(count_sql.str());
  }

  int offset = (page - 1) * page_size;
  std::ostringstream sql;
    sql << "SELECT d.server_name, d.cpu_name, d.timestamp, d.cpu_percent, "
        "d.usr_percent, d.system_percent, d.nice_percent, d.idle_percent, "
        "d.io_wait_percent, d.irq_percent, d.soft_irq_percent "
        "FROM server_cpu_core_detail d "
        "INNER JOIN ("
        "  SELECT cpu_name, MAX(timestamp) AS latest_ts "
        "  FROM server_cpu_core_detail "
        "  WHERE server_name='"
      << server_name << "' AND timestamp BETWEEN '" << start_time << "' AND '"
      << end_time
      << "' GROUP BY cpu_name"
        ") latest ON d.cpu_name = latest.cpu_name AND d.timestamp = latest.latest_ts "
        "WHERE d.server_name='"
      << server_name << "' "
        "ORDER BY d.cpu_name ASC LIMIT "
      << page_size << " OFFSET " << offset;

  if (mysql_query(conn_, sql.str().c_str()) != 0) {
    fastlog::file::get_logger(kManagerLoggerName)
        ->error("QueryManager: cpu core detail query failed: {}",
                mysql_error(conn_));
    return records;
  }

  MYSQL_RES *result = mysql_store_result(conn_);
  if (!result) {
    return records;
  }

  MYSQL_ROW row;
  while ((row = mysql_fetch_row(result))) {
    CpuCoreDetailRecord rec;
    int i = 0;
    rec.server_name = row[i++] ? row[i - 1] : "";
    rec.cpu_name = row[i++] ? row[i - 1] : "";
    rec.timestamp =
        row[i] ? parse_time(row[i]) : std::chrono::system_clock::now();
    i++;
    rec.cpu_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.usr_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.system_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.nice_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.idle_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.io_wait_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.irq_percent = row[i] ? std::atof(row[i]) : 0;
    i++;
    rec.soft_irq_percent = row[i] ? std::atof(row[i]) : 0;
    records.push_back(rec);
  }
  mysql_free_result(result);

  return records;
}

} // namespace monitor
