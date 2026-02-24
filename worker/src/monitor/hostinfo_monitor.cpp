#include "monitor/hostinfo_monitor.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include "monitor_info.pb.h"
#include "fastlog/fastlog.hpp"

namespace monitor {

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}
std::string HostInfoMonitor::get_hostname() {
  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname);
  }
  fastlog::file::get_logger(kWorkerLoggerName)->error("Failed to get hostname");
  // fastlog::console.error("Failed to get hostname");
  return "unknown";
}

std::string HostInfoMonitor::get_primary_ip_address() {
  struct ifaddrs* ifaddr = nullptr;
  struct ifaddrs* ifa = nullptr;
  std::string result;

  if (getifaddrs(&ifaddr) == -1) {
    return "";
  }

  // 遍历所有网络接口
  for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }

    // 只处理 IPv4 地址
    if (ifa->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    // 跳过 loopback 接口
    if (strcmp(ifa->ifa_name, "lo") == 0) {
      continue;
    }

    // 跳过 docker/虚拟网卡（通常以 docker、veth、br- 开头）
    std::string ifname(ifa->ifa_name);
    if (ifname.find("docker") == 0 ||
        ifname.find("veth") == 0 ||
        ifname.find("br-") == 0 ||
        ifname.find("virbr") == 0) {
      continue;
    }

    // 获取 IP 地址
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
    char ip_str[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &(addr->sin_addr), ip_str, sizeof(ip_str)) != nullptr) {
      result = ip_str;
      break;  // 找到第一个有效 IP 就返回
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

void HostInfoMonitor::update(monitor::proto::MonitorInfo* monitor_info) {
  if (!monitor_info) {
    return;
  }

  // 主机信息通常不变，只需获取一次并缓存
  if (!_info_cached) {
    _cached_hostname = get_hostname();
    _cached_ip = get_primary_ip_address();
    _info_cached = true;
  }

  // 填充 HostInfo 消息
  auto* host_info = monitor_info->mutable_host_info();
  host_info->set_hostname(_cached_hostname);
  host_info->set_ip_address(_cached_ip);
}

}