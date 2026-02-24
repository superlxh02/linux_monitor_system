#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include "fastlog/fastlog.hpp"
#include "rpc/monitor_pusher.hpp"

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

constexpr char kDefaultManagerAddress[] = "localhost:50051";
constexpr int kDefaultPushInterval = 10;  // 秒

void PrintUsage(const char* program) {
  auto* log = fastlog::file::get_logger(kWorkerLoggerName);
  if (log) {
    // fastlog::console.info(...);
    log->info("Usage: {} <manager_address> [interval_seconds]", program);
    log->info("  manager_address: 管理者服务器地址 (如 192.168.1.100:50051)");
    log->info("  interval_seconds: 推送间隔秒数 (默认 10)");
  }
}

int main(int argc, char* argv[]) {
  std::filesystem::path log_dir =  "/tmp/linux_monitor_worker_logs/worker";
  std::filesystem::create_directories(log_dir);
  std::string log_path = (log_dir / "worker.log").string();
  auto& log = fastlog::file::make_logger(kWorkerLoggerName, log_path);
  log.set_level(fastlog::LogLevel::Info);

  std::string manager_address = kDefaultManagerAddress;
  int interval_seconds = kDefaultPushInterval;

  // 解析命令行参数
  if (argc > 1) {
    manager_address = argv[1];
  }
  if (argc > 2) {
    interval_seconds = std::stoi(argv[2]);
    if (interval_seconds <= 0) {
      interval_seconds = kDefaultPushInterval;
    }
  }

  // fastlog::console.info("Starting Monitor Server (Push Mode)...\n");
  log.info("Starting Monitor Server (Push Mode)...\n");
  // fastlog::console.info("Manager address: {}", manager_address);
  log.info("Manager address: {}", manager_address);
  // fastlog::console.info("Push interval: {} seconds", interval_seconds);
  log.info("Push interval: {} seconds", interval_seconds);

  // 创建并启动推送器
  monitor::MonitorPusher pusher(manager_address, interval_seconds);
  pusher.start();

  // 主线程保持运行
  // fastlog::console.info("Press Ctrl+C to exit.");
  log.info("Press Ctrl+C to exit.");
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }

  return 0;
}
