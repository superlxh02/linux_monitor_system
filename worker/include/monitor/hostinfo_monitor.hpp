#pragma once

#include "monitor/monitor.hpp"
#include <string>

namespace monitor {



class HostInfoMonitor : public Monitor {
public:
  HostInfoMonitor() = default;
  ~HostInfoMonitor() override = default;

  void update(monitor::proto::MonitorInfo *monitor_info) override;
  void stop() override {}

private:
  /**
   * 获取主机名
   * 使用 gethostname() 系统调用
   * @return 主机名字符串
   */
  std::string get_hostname();

  /**
   * 获取主网卡 IP 地址
   * 遍历 /sys/class/net/ 目录，过滤 lo 和虚拟网卡，
   * 获取第一个物理网卡的 IPv4 地址
   * @return IP 地址字符串
   */
  std::string get_primary_ip_address();

  std::string _cached_hostname; // 缓存的主机名
  std::string _cached_ip;       // 缓存的 IP 地址
  bool _info_cached = false;    // 是否已缓存（主机信息通常不变）
};

} // namespace monitor
