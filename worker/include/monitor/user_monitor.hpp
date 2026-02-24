#pragma once

#include <string>
#include "monitor/monitor.hpp"

namespace monitor {

/**
 * 用户信息监控器
 * 
 * 通过系统调用获取当前进程的实际用户ID(UID)，
 * 然后解析 /etc/passwd 文件查找对应的用户名。
 * 
 * 这种方式比 getenv("USER") 更可靠，不依赖环境变量，
 * 在容器环境下也能准确获取用户信息。
 */
class UserMonitor : public Monitor {
 public:
  UserMonitor() = default;
  ~UserMonitor() override = default;

  void update(monitor::proto::MonitorInfo* monitor_info) override;
  void stop() override {}

 private:
  /**
   * 根据 UID 从 /etc/passwd 文件中查找用户名
   * 模拟 getpwuid() 的核心功能
   * 
   * @param uid 用户ID
   * @return 用户名，如果未找到返回空字符串
   */
  std::string get_username_by_uid(uid_t uid);
};

}  // namespace monitor
