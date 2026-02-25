# Manager 组件设计与实现

## 1. 类总览

| 类名 | 所在头文件 | 功能简述 |
|------|------------|----------|
| **HostManager** | `host_manager.hpp` | 管理多台主机的监控数据（推送模式）：接收 Worker 推送的 `MonitorInfo`，计算健康评分，写入 MySQL，提供最优主机查询。 |
| **QueryManager** | `query_manager.hpp` | 封装 MySQL 连接与所有查询逻辑：性能数据、趋势、异常、评分排序、最新评分、网络/磁盘/内存/软中断详情等。 |
| **GrpcServerImpl** | `rpc/grpc_server.hpp` | 实现 `GrpcManager` gRPC 服务：接收 Worker 的 `SetMonitorInfo` 推送，存储并触发回调；提供 `GetMonitorInfo`（保留接口）。 |
| **QueryServiceImpl** | `rpc/query_service.hpp` | 实现 `QueryService` gRPC 服务：将各类查询请求转换为对 `QueryManager` 的调用，并把结果封装为 protobuf 响应。 |
| **RpcClient** | `rpc/rpc_client.hpp` | RPC 客户端：连接指定 Manager 地址，通过 `GetMonitorInfo` 拉取监控数据（当前架构下 Worker 主动推送，此客户端为备用/扩展）。 |

辅助结构体（部分）：

| 结构体/枚举 | 说明 |
|-------------|------|
| **HostScore** | 主机标识 + `MonitorInfo` + 健康评分 + 时间戳。 |
| **HostData** | `MonitorInfo` + 接收时间戳，用于 GrpcServerImpl 内存缓存。 |
| **PerformanceRecord / AnomalyRecord / ServerScoreSummary / ClusterStats** | 查询结果与统计的结构化表示。 |
| **TimeRange / AnomalyThresholds / SortOrder / ServerStatus / ScoringProfile** | 查询参数与配置（含评分场景）。 |

---

## 2. 整体设计思路

- **角色**：Manager 是中心节点，负责汇聚各 Worker 的监控数据、持久化、评分与对外查询。
- **与 Worker 通信**：采用 **推送模式**。Worker 周期性调用 Manager 的 gRPC 接口 `SetMonitorInfo` 上报 `MonitorInfo`；Manager 不主动向 Worker 拉取。
- **数据流**：
  1. Worker → `SetMonitorInfo` → **GrpcServerImpl** 写入 `_host_data` 并调用 **data_received_callback**；
  2. 回调将数据交给 **HostManager::on_data_received**：计算评分、写入 MySQL 多张表、更新内存中的 `_host_scores`；
  3. 外部或前端通过 **QueryService** 的 gRPC 接口发起查询，**QueryServiceImpl** 转调 **QueryManager** 的 MySQL 查询，返回 protobuf。
- **MySQL**：HostManager 负责**写入**（性能、异常、详情等表）；QueryManager 持有连接并负责**所有读**（分页、时间范围、聚合、排序等）。
- **健康评分**：在 HostManager 内对每份收到的 `MonitorInfo` 计算默认评分（`BALANCED`）用于写库；查询阶段可按 `ScoringProfile` 动态重算并返回不同场景评分。

---

## 3. SQL 表设计

库名：`monitor_db`，字符集 `utf8mb4`，引擎 InnoDB。表结构定义见 `manager/sql/server.sql`。HostManager 每次收到 Worker 推送会向以下 5 张表各写一条或多条记录；QueryManager 的各类查询均基于这些表。

### 3.1 表总览

| 表名 | 用途 | 写入方 | 典型查询 |
|------|------|--------|----------|
| **server_performance** | 性能汇总主表，每机每采样一条 | HostManager::write_to_mysql | 性能分页、趋势聚合、异常检测、评分排序、最新评分 |
| **server_net_detail** | 按网卡的详细网络数据 | HostManager::write_to_mysql | 网络详情分页 |
| **server_softirq_detail** | 按 CPU 的软中断详细数据 | HostManager::write_to_mysql | 软中断详情分页 |
| **server_mem_detail** | 内存详细数据（各分项 + 变化率） | HostManager::write_to_mysql | 内存详情分页 |
| **server_disk_detail** | 按磁盘的 IO 详细数据 | HostManager::write_to_mysql | 磁盘详情分页 |

说明：**异常**（CPU/内存/磁盘超阈值、变化率突增）不单独建表，由 QueryManager 在 `server_performance` 上按条件筛选（如 `cpu_percent > threshold`）并组装为 `AnomalyRecord` 返回。

### 3.2 server_performance（主表）

- **主键**：`id` 自增。
- **维度**：`server_name`（主机标识）+ `timestamp`（采集时间）。
- **指标**：
  - CPU：`cpu_percent`、`usr_percent`、`system_percent`、`nice_percent`、`idle_percent`、`io_wait_percent`、`irq_percent`、`soft_irq_percent`；
  - 负载：`load_avg_1`、`load_avg_3`、`load_avg_15`；
  - 内存：`mem_used_percent`、`total`、`free`、`avail`（后三者为 GB）；
  - 磁盘：`disk_util_percent`（取各盘最大利用率）；
  - 网络：`send_rate`、`rcv_rate`（kB/s）；
  - 评分：`score`（0～100）；
  - 变化率：`*_rate` 字段（如 `cpu_percent_rate`、`mem_used_percent_rate`、`load_avg_1_rate`、`send_rate_rate`、`rcv_rate_rate` 等），用于趋势与异常判断。
- **索引**：`(server_name, timestamp)`、`(score)`，便于按主机+时间查询和按评分排序。

### 3.3 server_net_detail（网络详情）

- **维度**：`server_name` + `net_name`（网卡名）+ `timestamp`。每台机每块网卡每采样一条。
- **字段**：错误/丢弃计数 `err_in`、`err_out`、`drop_in`、`drop_out`；速率 `rcv_bytes_rate`、`rcv_packets_rate`、`snd_bytes_rate`、`snd_packets_rate`；以及对应变化率。
- **索引**：`(server_name, net_name, timestamp)`。

### 3.4 server_softirq_detail（软中断详情）

- **维度**：`server_name` + `cpu_name`（如 cpu0）+ `timestamp`。每台机每个 CPU 每采样一条。
- **字段**：各软中断计数 `hi`、`timer`、`net_tx`、`net_rx`、`block`、`irq_poll`、`tasklet`、`sched`、`hrtimer`、`rcu`，以及对应 `*_rate` 变化率。
- **索引**：`(server_name, cpu_name, timestamp)`。

### 3.5 server_mem_detail（内存详情）

- **维度**：`server_name` + `timestamp`。每台机每采样一条。
- **字段**：内存分项（单位 GB）如 `total`、`free`、`avail`、`buffers`、`cached`、`active`、`inactive`、`dirty`、`anon_pages`、`mapped` 等，以及各字段的 `*_rate` 变化率。
- **索引**：`(server_name, timestamp)`、`(total, free, avail)`。

### 3.6 server_disk_detail（磁盘详情）

- **维度**：`server_name` + `disk_name`（设备名）+ `timestamp`。每台机每块盘每采样一条。
- **字段**：原始计数器 `reads`、`writes`、`sectors_read`、`sectors_written`、`read_time_ms`、`write_time_ms`、`io_in_progress`、`io_time_ms`、`weighted_io_time_ms`；派生指标 `read_bytes_per_sec`、`write_bytes_per_sec`、`read_iops`、`write_iops`、`avg_read_latency_ms`、`avg_write_latency_ms`、`util_percent`；以及对应 `*_rate`。
- **索引**：`(server_name, disk_name, timestamp)`。

### 3.7 与写入/查询的对应关系

- **HostManager::write_to_mysql**：对一次推送，先插 1 条 `server_performance`（汇总 + 变化率 + score），再按网卡数插 `server_net_detail`、按 CPU 数插 `server_softirq_detail`、插 1 条 `server_mem_detail`、按磁盘数插 `server_disk_detail`。
- **QueryManager**：性能/趋势/评分/最新评分均查 `server_performance`；异常查询在 `server_performance` 上按阈值条件筛选；详情类接口分别查 `server_net_detail`、`server_softirq_detail`、`server_mem_detail`、`server_disk_detail`，均支持 `server_name` + 时间范围 + 分页。

---

## 4. 重要接口源码详细分析

### 4.1 与 Worker 的通信接口与封装

#### 4.1.1 协议与服务定义

- 通信协议：gRPC + protobuf。
- 接收端服务：`monitor::proto::GrpcManager`，实现类为 **GrpcServerImpl**。
- 核心 RPC：**SetMonitorInfo**（Worker 推送）、GetMonitorInfo（保留，可返回缓存中某条数据）。

#### 4.1.2 GrpcServerImpl：接收推送并回调

```cpp
// manager/include/rpc/grpc_server.hpp
class GrpcServerImpl : public monitor::proto::GrpcManager::Service {
 public:
  ::grpc::Status SetMonitorInfo(::grpc::ServerContext* context,
                                const ::monitor::proto::MonitorInfo* request,
                                ::google::protobuf::Empty* response) override;
  void set_data_received_callback(data_received_callback_t callback);
  // ...
 private:
  std::mutex _mtx;
  std::unordered_map<std::string, HostData> _host_data;
  data_received_callback_t _callback;
};
```

**SetMonitorInfo 实现要点**（`manager/src/rpc/grpc_server.cpp`）：

1. 校验 `request` 非空，从 `request->name()` 或 `request->host_info().hostname()` 得到主机标识；
2. 加锁写入 `_host_data[hostname] = {*request, now}`；
3. 若已设置 `_callback`，则调用 `_callback(*request)`，将数据交给 HostManager。

这样设计的目的：**解耦“接收存储”与“业务处理”**。GrpcServer 只负责收包与缓存，具体落库、评分、选优由 HostManager 在回调中完成。

#### 4.1.3 Manager 主流程中如何挂接

```cpp
// manager/manager.cpp
monitor::GrpcServerImpl service;
monitor::HostManager mgr;
service.set_data_received_callback(
    [&mgr](const monitor::proto::MonitorInfo& info) {
      mgr.on_data_received(info);
    });
mgr.start();
// ...
builder.RegisterService(&service);
builder.RegisterService(&query_service);  // 查询服务同进程
```

Worker 侧对应逻辑：**MonitorPusher** 周期调用 `_stub->SetMonitorInfo(&context, info, &response)`，将 **MetricCollector** 采集的 `MonitorInfo` 推送到 Manager。

---

### 4.2 封装查询接口（QueryServiceImpl → QueryManager）

查询链路：**gRPC 请求 → QueryServiceImpl → QueryManager → MySQL → 填充 protobuf 响应**。

#### 4.2.1 QueryServiceImpl 的职责

- 持有 `QueryManager* _query_manager`，不持有 MySQL 连接。
- 将 protobuf 的 `TimeRange`、分页等转为 C++ 的 `TimeRange`、`page`、`page_size`；
- 解析请求中的 `scoring_profile`（若未传则默认 `BALANCED`）；
- 调用 QueryManager 的对应方法（如 `query_performance`、`query_trend`、`query_anomaly`、`query_score_rank`、`query_latest_score`、各类 `query_*_detail`）；
- 将返回的 `PerformanceRecord`、`ServerScoreSummary` 等转为 protobuf 并写回 response。

评分相关接口（`QueryPerformance`、`QueryTrend`、`QueryScoreRank`、`QueryLatestScore`）均支持按场景返回分数。

#### 4.2.2 示例：QueryPerformance

```cpp
// manager/src/rpc/query_service.cpp
::grpc::Status QueryServiceImpl::QueryPerformance(
    ::grpc::ServerContext* context,
    const ::monitor::proto::QueryPerformanceRequest* request,
    ::monitor::proto::QueryPerformanceResponse* response) {
  if (!_query_manager) return grpc::Status(..., "Query manager not initialized");
  TimeRange time_range = convert_time_range(request->time_range());
  if (!_query_manager->validate_timerange(time_range))
    return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid time range...");

  int page = request->pagination().page();
  int page_size = request->pagination().page_size();
  int total_count = 0;
    auto scoring_profile = convert_scoring_profile(request->scoring_profile());
    auto records = _query_manager->query_performance(
      request->server_name(), time_range, page, page_size, &total_count,
      scoring_profile);

  for (const auto& rec : records) {
    auto* proto_rec = response->add_records();
    proto_rec->set_server_name(rec.server_name);
    set_timestamp(proto_rec->mutable_timestamp(), rec.timestamp);
    proto_rec->set_cpu_percent(rec.cpu_percent);
    // ... 其余字段逐一 set_*
  }
  response->set_total_count(total_count);
  response->set_page(page);
  response->set_page_size(page_size);
  response->set_scoring_profile(request->scoring_profile());
  return grpc::Status::OK;
}
```

其他查询接口（QueryTrend、QueryAnomaly、QueryScoreRank、QueryLatestScore、QueryNetDetail 等）遵循同一模式：**参数校验 → QueryManager 查 MySQL → 结果转 protobuf**。

---

### 4.3 MySQL 接口（QueryManager）

#### 4.3.1 连接管理

```cpp
// manager/src/query_manager.cpp
bool QueryManager::init(const std::string& host, const std::string& user,
                        const std::string& password, const std::string& database) {
  std::lock_guard<std::mutex> lock(_mtx);
  if (_initialized) return true;
  conn_ = mysql_init(nullptr);
  if (!conn_) return false;
  if (!mysql_real_connect(conn_, host.c_str(), user.c_str(), password.c_str(),
                          database.c_str(), 0, nullptr, 0)) {
    mysql_close(conn_);
    conn_ = nullptr;
    return false;
  }
  mysql_set_character_set(conn_, "utf8mb4");
  _initialized = true;
  return true;
}
```

- 单连接 `MYSQL* conn_`，用 `_mtx` 保护并发访问。
- 所有查询方法内都会 `lock_guard` 并检查 `_initialized && conn_`，保证线程安全。

#### 4.3.2 典型查询：query_performance

- 校验 `time_range`、规范化 `page`/`page_size`；
- 用 `format_time` 将 `TimeRange` 转为 SQL 可用的时间字符串；
- 先执行 `COUNT(*)` 得到 `total_count`（用于分页总数）；
- 再执行主查询：`SELECT server_name, timestamp, cpu_percent, ... FROM server_performance WHERE server_name=? AND timestamp BETWEEN ? AND ? ORDER BY timestamp DESC LIMIT ? OFFSET ?`；
- `mysql_query` → `mysql_store_result` → 逐行 `mysql_fetch_row`，按列顺序解析到 `PerformanceRecord`，并调用 `parse_time` 解析时间戳；
- 返回 `std::vector<PerformanceRecord>`，并通过出参 `total_count` 返回总数。

其他接口（如 `query_trend` 带时间桶聚合、`query_anomaly` 按阈值过滤、`query_score_rank` 排序、`query_net_detail` 等）结构类似：组 SQL、执行、将结果集映射到对应结构体并返回。

---

### 4.4 健康评分算法（多场景）

#### 4.4.1 调用位置

在 **HostManager::on_data_received** 中，对每份收到的 `MonitorInfo` 调用：

```cpp
double score = calc_scores(info, ScoringProfile::BALANCED);
```

结果用于：写入内存 `_host_scores`、写入 MySQL、以及通过 `get_best_host()` 做负载均衡选优。

查询阶段（`QueryManager`）会根据请求中的 `ScoringProfile` 对查询结果进行**动态重算分数**，以满足不同业务场景。

#### 4.4.2 算法是什么（calc_scores 源码逻辑）

设计目标：提供可切换的评分体系，使同一份原始监控数据在不同业务假设下得到更合理的排序结果（例如高并发、I/O 密集、内存敏感）。

**评分体系与权重**（`ScoringProfile`）：

| 评分体系 | CPU | 内存 | 负载 | 磁盘 | 网络 | 负载系数 N |
|---|---:|---:|---:|---:|---:|---:|
| `BALANCED` | 0.35 | 0.30 | 0.15 | 0.15 | 0.05 | 1.5 |
| `HIGH_CONCURRENCY` | 0.45 | 0.25 | 0.15 | 0.10 | 0.05 | 1.2 |
| `IO_INTENSIVE` | 0.20 | 0.15 | 0.20 | 0.35 | 0.10 | 2.0 |
| `MEMORY_SENSITIVE` | 0.20 | 0.45 | 0.15 | 0.10 | 0.10 | 1.5 |

**步骤简述**：

1. **取原始指标**  
   从 `info` 中读取：`cpu_percent`、`load_avg_1`、`mem_percent`（used_percent）、各磁盘 `util_percent` 取最大为 `disk_util`、首条网卡 `rcv_rate`/`send_rate`。CPU 核数用于负载归一化（见下）。

2. **单项得分（反向归一化）**  
   - 使用率类（CPU、内存、磁盘）：越高越差，故 `score = 1 - value/100`，并 clamp 到 [0,1]。  
  - 负载：`load_score = 1 - load_avg_1 / (cpu_cores * N)`，`N` 由评分体系决定。  
  - 网络：以 1Gbps（125000000 B/s）为参考，`net_*_score = 1 - rate / max_bandwidth`，收发各算一个再取平均得到 `net_score`。

3. **加权总分**  
  `score = cpu_score*w_cpu + mem_score*w_mem + load_score*w_load + disk_score*w_disk + net_score*w_net`，再乘以 100，并限制在 [0, 100]。

#### 4.4.3 为什么要这样设计

- **按业务切换权重**：不同工作负载关注点不同，支持多套权重避免“一套分数走天下”。
- **写入与查询解耦**：写库阶段保留默认 `BALANCED` 分数，查询阶段可按请求动态重算并排序，不破坏历史数据结构。
- **兼容旧调用**：未传 `scoring_profile` 时默认 `BALANCED`，旧客户端无需改动即可继续使用。
- **反向归一化**：所有指标都转为“越空闲越好”的 0～1 分，再线性加权，保证分数直观（高=健康、低=繁忙），便于排序和阈值告警。

这样，Manager 既能用 **calc_scores** 做实时选优（get_best_host），又能在 QueryService 中按 `ScoringProfile` 输出“面向不同业务场景”的评分结果。
