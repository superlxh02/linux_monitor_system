#include <grpc/grpc.h>
#include <grpcpp/server_builder.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "fastlog/fastlog.hpp"
#include "host_manager.hpp"
#include "query_manager.hpp"
#include "rpc/grpc_server.hpp"
#include "rpc/query_service.hpp"

namespace {
constexpr char kManagerLoggerName[] = "manager_file_logger";
}

constexpr char kDefaultListenAddress[] = "0.0.0.0:50051";
constexpr char kDefaultMysqlHost[] = "127.0.0.1";
constexpr char kDefaultMysqlUser[] = "monitor";
constexpr char kDefaultMysqlPass[] = "monitor123";
constexpr char kDefaultMysqlDb[] = "monitor_db";

int main(int argc, char* argv[]) {
  std::filesystem::path log_dir = "/tmp/linux_monitor_worker_logs/manager";
  std::filesystem::create_directories(log_dir);
  std::string log_path = (log_dir / "manager.log").string();
  auto& log = fastlog::file::make_logger(kManagerLoggerName, log_path);
  log.set_level(fastlog::LogLevel::Info);

  std::string listen_address = kDefaultListenAddress;

  // 解析命令行参数
  if (argc > 1) {
    listen_address = argv[1];
  }

  // fastlog::console.info("Starting Monitor Client (Manager Mode)...");
  log.info("Starting Monitor Client (Manager Mode)...");
  // fastlog::console.info("Listening on: {}", listen_address);
  log.info("Listening on: {}", listen_address);

  // 创建 gRPC 服务
  monitor::GrpcServerImpl service;

  // 创建 HostManager 并设置回调
  monitor::HostManager mgr;
  service.set_data_received_callback(
      [&mgr](const monitor::proto::MonitorInfo& info) {
        mgr.on_data_received(info);
      });

  // 启动 HostManager 后台处理
  mgr.start();

  // 创建 QueryManager 并初始化
  monitor::QueryManager query_mgr;
  if (query_mgr.init(kDefaultMysqlHost, kDefaultMysqlUser, kDefaultMysqlPass,
                     kDefaultMysqlDb)) {
    // fastlog::console.info("QueryManager initialized successfully");
    log.info("QueryManager initialized successfully");
  } else {
    // fastlog::console.error(...);
    log.error(
        "QueryManager initialization failed, query service may be unavailable");
  }

  // 创建查询服务
  monitor::QueryServiceImpl query_service(&query_mgr);

  // 启动 gRPC 服务器
  grpc::ServerBuilder builder;
  builder.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  builder.RegisterService(&query_service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  // fastlog::console.info("Monitor Client listening on {}", listen_address);
  log.info("Monitor Client listening on {}", listen_address);
  // fastlog::console.info("Waiting for workers to push data...");
  log.info("Waiting for workers to push data...");
  // fastlog::console.info("Query service available for performance data queries");
  log.info("Query service available for performance data queries");

  // 定期手动刷新文件日志，避免缓冲区未满时日志不落盘
  std::atomic<bool> running{true};
  std::thread flush_thread([&running]() {
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      if (!running) break;
      auto* lg = fastlog::file::get_logger(kManagerLoggerName);
      if (lg) lg->flush();
    }
  });

  server->Wait();
  running = false;
  if (flush_thread.joinable()) flush_thread.join();

  return 0;
}
