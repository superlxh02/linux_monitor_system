# Linux 监控系统可视化面板（web_dashboard_example）

这是 Linux 监控系统的实时 Web 监控面板，负责将 Manager 的查询能力可视化展示。
面板后端为 Node.js（Express + gRPC + Socket.IO），可作为主项目的前端示例与 BFF 参考实现。

## 功能说明
- **实时总览**：展示所有在线服务器的健康评分与关键指标（CPU、内存、磁盘、负载）。
- **评分体系切换**：支持 `BALANCED`、`HIGH_CONCURRENCY`、`IO_INTENSIVE`、`MEMORY_SENSITIVE`，可按业务场景动态重算展示分数。
- **CPU 细分监控**：CPU 页面支持每节点各核心的柱状负载视图（类似 btop）并保留总 CPU 负载曲线（柱状在上、曲线在下）。
- **CPU 关键指标矩阵**：按每个核心展示总使用率、用户态、内核态、IO等待、软中断、空闲占比，便于性能定位。
- **网络流量监控项**：按“输入 / 输出 / 总流量”三路展示，每一路都提供趋势图 + 明细表，并带分位阈值告警（P90/P98）。
- **磁盘 IO 监控项**：按“读 / 写 / 总流量”三路展示，每一路都提供趋势图 + 明细表，并带分位阈值告警（P90/P98）。
- **节点详情**：点击节点可查看 CPU、内存、磁盘、网络等历史趋势图。
- **完整查询页**：内置评分排行、趋势聚合、异常记录、网络/磁盘/内存/软中断详情查询。
- **查询端口清单**：单独页面展示当前暴露的 gRPC / HTTP / Socket 查询入口。
- **现代化界面**：基于 React + Material UI 的深色主题看板。
- **实时推送**：通过 Socket.IO 持续推送总览数据更新。

## 运行前提
- Node.js（建议 v14+）
- C++ Manager 已启动，并可通过 `127.0.0.1:50051` 访问

## 启动方式

1. 安装依赖：
   ```bash
   npm install
   ```

2. 启动面板服务：
   ```bash
   npm start
   ```
   或：
   ```bash
   node server.js
   ```

3. 浏览器访问：
   [http://localhost:3000](http://localhost:3000)

## 架构说明
- **BFF 层**：`server.js` 使用 Express 提供 HTTP 接口，并作为 gRPC 客户端连接 Manager 的 `QueryService`。
- **前端层**：`public/app.js` 为单文件 React 应用，通过 CDN 引入 React/MUI/Recharts。

## 对外接口（BFF）

面板默认监听 `http://0.0.0.0:3000`，转发到 Manager 的 `monitor.proto.QueryService`（默认 `127.0.0.1:50051`）。

### HTTP 接口

- `GET /api/overview?scoring_profile=BALANCED` → `QueryLatestScore`
- `GET /api/performance/:serverName?hours=1&scoring_profile=BALANCED` → `QueryPerformance`
- `GET /api/rank?order=DESC&page=1&page_size=20&scoring_profile=BALANCED` → `QueryScoreRank`
- `GET /api/anomaly?server_name=&hours=24&cpu_threshold=80&mem_threshold=90&disk_threshold=85&change_rate_threshold=0.5&page=1&page_size=50` → `QueryAnomaly`
- `GET /api/trend/:serverName?hours=1&interval_seconds=300&scoring_profile=BALANCED` → `QueryTrend`
- `GET /api/net-detail?server_name=&hours=24&page=1&page_size=50` → `QueryNetDetail`
- `GET /api/disk-detail?server_name=&hours=24&page=1&page_size=50` → `QueryDiskDetail`
- `GET /api/mem-detail?server_name=&hours=24&page=1&page_size=50` → `QueryMemDetail`
- `GET /api/softirq-detail?server_name=&hours=24&page=1&page_size=50` → `QuerySoftIrqDetail`
- `GET /api/cpu-core-detail?server_name=&hours=24` → MySQL `server_cpu_core_detail`（每节点每核心最新负载）
- `GET /api/query-endpoints` → 返回面板用的查询端口清单

说明：`/api/cpu-core-detail` 依赖 `manager/sql/server.sql` 中的 `server_cpu_core_detail` 表，请确保已执行最新建表脚本。

`scoring_profile` 可选值：

- `BALANCED`（默认）
- `HIGH_CONCURRENCY`
- `IO_INTENSIVE`
- `MEMORY_SENSITIVE`

### Socket 通道

- `overview_update`（每 2 秒推送一次，对应 `QueryLatestScore` 轮询结果）
