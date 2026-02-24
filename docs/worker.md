# Worker 组件设计与实现

## 1. 类总览

| 类名                        | 所在头文件                         | 功能简述                                                                                                                             |
| --------------------------- | ---------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| **Monitor**           | `monitor/monitor.hpp`            | 监控抽象基类：定义 `update(MonitorInfo*)` 与 `stop()`，所有具体监控器实现该接口。                                                |
| **MetricCollector**   | `monitor/metric_collector.hpp`   | 指标采集入口：持有多个 `Monitor` 实例，`collect_all` 时依次调用各 Monitor 的 `update`，将结果汇总到一份 `MonitorInfo`。      |
| **NetEbpfMonitor**    | `monitor/net_ebpf_monitor.hpp`   | 基于 eBPF TC hook 的网络流量监控：加载 BPF 程序、挂载到各网卡 ingress/egress，从 BPF map 读字节/包计数并算速率写入 `MonitorInfo`。 |
| **CpuStatMonitor**    | `monitor/cpustate_monitor.hpp`   | CPU 状态监控：通过 mmap 读取内核模块 `/dev/cpu_stat_monitor` 的数据，计算各 CPU 使用率并写入 `MonitorInfo`。                     |
| **CpuSoftIrqMonitor** | `monitor/cpusoftirq_monitor.hpp` | 软中断监控：通过 mmap 读取内核模块 `/dev/cpu_softirq_monitor` 的数据，计算各 CPU 软中断速率并写入 `MonitorInfo`。                |
| **CpuLoadMonitor**    | `monitor/cpuload_monitor.hpp`    | CPU 负载：读取 `/proc/loadavg`，填充 `MonitorInfo::cpu_load`。                                                                   |
| **MemoryMonitor**     | `monitor/memory_monitor.hpp`     | 内存：解析 `/proc/meminfo`，填充 `MonitorInfo::mem_info`。                                                                       |
| **DiskMonitor**       | `monitor/disk_monitor.hpp`       | 磁盘 IO：解析 `/proc/diskstats` 等，填充 `MonitorInfo::disk_info`。                                                              |
| **HostInfoMonitor**   | `monitor/hostinfo_monitor.hpp`   | 主机信息：主机名、IP 等，填充 `MonitorInfo::host_info`。                                                                           |
| **MonitorPusher**     | `rpc/monitor_pusher.hpp`         | 推送逻辑：周期调用 `MetricCollector::collect_all` 得到 `MonitorInfo`，通过 gRPC `SetMonitorInfo` 推送到 Manager。              |
| **GrpcManagerImpl**   | `rpc/grpc_manager_impl.hpp`      | 本地 gRPC 服务实现（若 Worker 同时对外提供 GetMonitorInfo 时使用，当前主流程以推送为主）。                                           |

---

## 2. 整体设计思路

- **角色**：Worker 部署在被监控主机上，负责**采集本机指标**并**主动上报**给 Manager。
- **采集**：所有指标统一由 **MetricCollector** 汇聚。Collector 内部维护一组 **Monitor** 子类实例（CPU 状态、软中断、负载、内存、网络 eBPF、磁盘、主机信息等），每次 `collect_all(monitor_info)` 时对同一份 `MonitorInfo*` 依次调用各 Monitor 的 `update(monitor_info)`，各子类只往 `monitor_info` 里追加自己负责的字段（如 `add_cpu_stat`、`add_net_info`），互不覆盖。
- **上报**：**MonitorPusher** 在独立线程中周期执行：`collect_all(&info)` → `_stub->SetMonitorInfo(&context, info, &response)`，将完整 `MonitorInfo` 推送到 Manager 的 gRPC 服务。
- **内核数据来源**：
  - **eBPF**：网络流量通过 TC 挂载的 BPF 程序在内核统计，用户态只读 BPF map。
  - **kmod**：CPU 状态与软中断通过两个内核模块暴露的 **mmap 字符设备** 读取，内核用高精度定时器周期更新共享内存，用户态 mmap 后直接读结构体数组。

---

## 3. 重要接口与源码详细分析

### 3.1 Monitor 的共同编写思路

#### 3.1.1 update 接口的共同编写思路

**简明三步**：每个监控器的 `update` 可以概括为：

1. **读取数据源** — 从“文件”读入原始数据（这里的“文件”是广义的：包括 `/proc` 下文本、设备节点如 `/dev/cpu_stat_monitor` 的 mmap、以及 eBPF map 的读）。
2. **将结果写入结构体** — 把读到的内容解析成内存中的结构体或局部变量（如 `mem_info`、`DiskSample`、`struct cpu_stat*` 等），便于计算和差分。
3. **填充 request 属于监控器自己的部分** — 只往 `monitor_info` 里填本 Monitor 负责的那一块（`add_cpu_stat()`、`mutable_mem_info()`、`add_net_info()` 等），不碰其他字段。

下面按实现细节展开为更细的步骤，便于处理边界情况（前置检查、差分、cache、资源释放）：

| 步骤                               | 说明                                                                                                 | 典型做法                                                                                                                                                                                                                                    |
| ---------------------------------- | ---------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **1. 前置检查**              | 判断数据源是否可用，不可用则直接 return 或走降级路径。                                               | 设备类：`open("/dev/xxx")` 失败且为 ENOENT 时置 `_device_missing`，并做节流（如 30 秒内不再重试）。eBPF 类：检查 `_loaded && _map_fd >= 0`。                                                                                          |
| **2. 获取原始数据**          | 从内核/文件读入“当前快照”。                                                                        | 设备：`mmap` 后按与内核约定好的结构体解析。eBPF：`bpf_map_get_next_key` + `bpf_map_lookup_elem` 遍历 map。文件：读 `/proc/loadavg`、`/proc/meminfo`、`/proc/diskstats` 等并解析。                                               |
| **3. 差分与计算**            | 需要**速率/百分比**的指标：用“本次 − 上次”除以时间差；首次或无 cache 时填 0 或瞬时值。      | 累计型：用成员变量或静态 map 存“上次采样”和（可选）时间戳；`delta = curr - last`，`rate = delta / dt` 或 `percent = delta_busy / delta_total * 100`。瞬时型（如 loadavg、meminfo）：直接换算后写入，无需 cache。                    |
| **4. 只写本 Monitor 的字段** | 仅调用与本 Monitor 对应的 `add_*` 或 `mutable_*`/`set_*`，不修改 `monitor_info` 的其他部分。 | CpuStatMonitor →`add_cpu_stat()`；CpuSoftIrqMonitor → `add_soft_irq()`；NetEbpfMonitor → `add_net_info()`；MemoryMonitor → `mutable_mem_info()`；DiskMonitor → `add_disk_info()`；CpuLoadMonitor → `mutable_cpu_load()`。 |
| **5. 更新 cache**            | 若本轮用到了“上次采样”，在写完后把本次采样（及时间戳）写回成员/静态变量，供下次 `update` 使用。  | CpuStatMonitor 的 `_cpu_stat_map`、CpuSoftIrqMonitor 的 `_cpu_softirqs`、NetEbpfMonitor 的 `_cache`、DiskMonitor 的 `last_samples`/`last_time`。                                                                                  |
| **6. 资源释放**              | 若在本次 `update` 内 open/mmap，在同一轮结束前 munmap/close，避免 fd 泄漏。                        | 设备类：`munmap(addr, size); close(fd);`。eBPF 一般不在此处释放，在 `stop()` 里统一 cleanup。                                                                                                                                           |

**按数据源与是否差分的分类**：

- **累计型 + 需速率/百分比**（有 cache、做差分）：CpuStatMonitor、CpuSoftIrqMonitor、NetEbpfMonitor、DiskMonitor。
- **瞬时型**（无 cache、直接填）：CpuLoadMonitor、MemoryMonitor、HostInfoMonitor。

按上表实现即可保证：接口一致、只写自己的字段、需要速率的指标用差分、失败可降级且不破坏其他 Monitor 的数据。

#### 3.1.2 接口约定与组装

1. **继承 Monitor 接口**

   - 实现 `void update(monitor::proto::MonitorInfo* monitor_info) override`：按 3.1.1 的步骤从数据源读数据、计算、只写本 Monitor 的字段。
   - 实现 `void stop() override`：释放长期占用的资源（eBPF 卸载、若存在长驻 fd 则关闭），无则空实现。
2. **单一职责**每个 Monitor 只填自己对应的消息部分（见上表步骤 4），这样新增或替换某种采集方式只需增删/替换对应 Monitor，不影响其他项。
3. **MetricCollector 的组装与调用**

   ```cpp
   // worker/src/monitor/metric_collector.cpp
   MetricCollector::MetricCollector() {
     gethostname(...);  // _hostname
     _monitors.push_back(std::make_unique<CpuLoadMonitor>());
     _monitors.push_back(std::make_unique<CpuStatMonitor>());
     _monitors.push_back(std::make_unique<CpuSoftIrqMonitor>());
     _monitors.push_back(std::make_unique<MemoryMonitor>());
     _monitors.push_back(std::make_unique<NetEbpfMonitor>());
     _monitors.push_back(std::make_unique<DiskMonitor>());
     _monitors.push_back(std::make_unique<HostInfoMonitor>());
   }
   void MetricCollector::collect_all(monitor::proto::MonitorInfo* monitor_info) {
     monitor_info->set_name(_hostname);
     for (auto& monitor : _monitors)
       monitor->update(monitor_info);
   }
   ```

   顺序可调，只要依赖关系允许（例如 HostInfo 通常先设 name，其余只追加）。析构时对每个 monitor 调用 `stop()`。

---

### 3.2 eBPF 模块的实现与作用

#### 3.2.1 目标与选型

- **目标**：按网卡统计收/发字节数与包数，并得到速率（bytes/s、packets/s），供 Manager 做健康评分与历史分析。
- **选型**：使用 **TC (Traffic Control) hook** 的 ingress/egress，在数据包进入/离开协议栈时挂载 eBPF 程序，能拿到 `struct __sk_buff` 的 `ifindex` 和 `len`，按网卡分别累计。TC 位于 L2/L3 边界，不破坏报文，返回 `TC_ACT_OK` 即可透传。

#### 3.2.2 内核态：net_stats.bpf.c

**BPF Map**

- 类型：`BPF_MAP_TYPE_HASH`。
- Key：`__u32`（网卡 ifindex）。
- Value：`struct net_stats { rcv_bytes, rcv_packets, snd_bytes, snd_packets }`。
- 与用户态共享同一结构定义，用户态通过 `bpf_map_get_next_key` + `bpf_map_lookup_elem` 遍历所有网卡统计。

**辅助函数 update_stats**

- 根据 `bpf_map_lookup_elem` 是否命中，对新 key 做初始化（只写当前方向字节/包），对已有 key 用 `__sync_fetch_and_add` 原子累加，保证多 CPU 并发安全。

**TC 程序**

- `SEC("tc/ingress")`：`tc_ingress(struct __sk_buff *skb)`：取 `skb->ifindex`、`skb->len`，过滤 ifindex/len 为 0 后调用 `update_stats(ifindex, len, true)`（rx），返回 `TC_ACT_OK`。
- `SEC("tc/egress")`：`tc_egress(struct __sk_buff *skb)`：同理，`update_stats(ifindex, len, false)`（tx），返回 `TC_ACT_OK`。

**编译与加载**

- 由 CMake 使用 clang `-target bpf` 编译为 `net_stats.bpf.o`，再用 `bpftool gen skeleton` 生成 `net_stats.skel.h`，用户态通过 skeleton API 打开、加载、挂载。

#### 3.2.3 用户态：NetEbpfMonitor

**初始化 init_ebpf()**

1. `net_stats_bpf__open()` / `net_stats_bpf__load()` 加载 BPF 程序。
2. 取 `net_stats_map` 的 fd 保存为 `_map_fd`。
3. 枚举本机网卡：读 `/sys/class/net` 得到各 ifindex，跳过 `lo`。
4. 对每个网卡：用 `tc qdisc add dev <ifname> clsact` 确保有 clsact；用 `bpf_tc_hook_create` + `bpf_tc_attach` 把 skeleton 里的 `tc_ingress` / `tc_egress` 分别挂到该 ifindex 的 BPF_TC_INGRESS 和 BPF_TC_EGRESS。
5. 记录已挂载的 ifindex 到 `_attached_ifindexes_`，并保存 skeleton 指针供析构时 `net_stats_bpf__destroy`。

**每次 update()**

1. 若未加载或 `_map_fd < 0` 则直接返回。
2. 用 `bpf_map_get_next_key` 从 0 开始遍历 map，对每个 key 用 `bpf_map_lookup_elem` 取出当前 `net_stats`。
3. 用 `get_ifname(ifindex)`（带缓存）得到网卡名，跳过空名或 `lo`。
4. 在 `_cache` 中查该 ifindex 的上一次统计与时间戳；若有则用本次与上次的差值除以时间差得到速率（bytes/s、packets/s），并处理计数器回绕。
5. 向 `monitor_info->add_net_info()` 写入 name、rcv_rate、send_rate、rcv_packets_rate、send_packets_rate。
6. 更新 `_cache` 为当前统计与当前时间，供下次计算速率。

**清理 cleanup_ebpf()**
对 `_attached_ifindexes_` 中每个 ifindex 做 `bpf_tc_detach`（ingress/egress 各一次），然后 `net_stats_bpf__destroy(skel)`，清空 `_attached_ifindexes_` 和 `_map_fd`。

**作用小结**：在内核按包精确统计每网卡收发量与包数，用户态只做周期读 map + 差分算速率，开销小且与协议无关；需 root 或 CAP_BPF/CAP_NET_ADMIN，若加载失败则本 Monitor 不填网络数据（可后续扩展为回退到 /proc/net/dev 等）。

---

### 3.3 kmod 模块的实现与作用

两个内核模块采用同一套架构：**内核分配共享结构体数组 → 高精度定时器周期从内核统计源更新 → 字符设备 mmap 暴露给用户态**。用户态 Monitor 打开设备、mmap、按与内核一致的结构体解析，并做差分计算（速率或百分比）。

#### 3.3.1 cpu_stat_collector（CPU 状态）

**作用**：提供每 CPU 的 user/nice/system/idle/iowait/irq/softirq/steal/guest/guest_nice 等时间的**累计值**（单位与内核一致，用户态再按时间差算使用率）。

**数据来源**：

- 使用 `kcpustat_cpu(cpu).cpustat[]`（即 `kernel_cpustat`）的 CPUTIME_* 项。
- idle/iowait 在支持 NO_HZ 时用 `get_cpu_idle_time_us` / `get_cpu_iowait_time_us` 得到更准确值，否则用 `cpustat` 并做纳秒到 jiffies 的转换。

**实现要点**：

- 分配 `struct cpu_stat[MAX_CPUS]`（PAGE_ALIGN），与用户态 `monitor_struct.h` 中的 `struct cpu_stat` 一致。
- 注册字符设备 `DEVICE_NAME "cpu_stat_monitor"`，实现 `open/release/mmap`；mmap 时 `remap_pfn_range` 映射上述数组，`pgprot_noncached` 避免缓存导致用户态读不到最新值。
- 使用 `hrtimer` 每 1 秒调用 `update_cpu_stats()`：遍历每个 possible CPU，从 `kcpustat_cpu` 读入并写入 `cpu_stat_data[idx]`，末尾用 `cpu_name[0]='\0'` 标记有效条数结束。

**用户态 CpuStatMonitor**：

- `open("/dev/cpu_stat_monitor")`，mmap 为 `struct cpu_stat*`。
- 遍历直到 `cpu_name[0]=='\0'`，与上次采样 `_cpu_stat_map` 做差：
  - 总时间差 = 本次 (user+system+idle+...) - 上次总和；
  - 各状态占比 = (本次该状态 - 上次该状态) / 总时间差 * 100。
- 填充 `monitor_info->add_cpu_stat()` 的 cpu_name、cpu_percent、usr_percent、system_percent 等，并更新 `_cpu_stat_map` 缓存。
- 若设备不存在（如模块未加载），设置 `_device_missing` 并在一段时间内不再频繁 open，避免刷日志。

#### 3.3.2 softirq_collector（软中断）

**作用**：提供每 CPU 的各类软中断**累计次数**（HI、TIMER、NET_TX、NET_RX、BLOCK、IRQ_POLL、TASKLET、SCHED、HRTIMER、RCU），用户态按时间差算每秒次数。

**数据来源**：`kstat_softirqs_cpu(softirq_id, cpu)`。

**实现要点**：

- 分配 `struct softirq_stat[MAX_CPUS]`（与用户态 `monitor_struct.h` 的 `struct softirq_stat` 一致），注册设备 `DEVICE_NAME "cpu_softirq_monitor"`，mmap 方式同 cpu_stat。
- `hrtimer` 每 1 秒调用 `update_softirq_stats()`：对每个 CPU 写 cpu_name 及各 `kstat_softirqs_cpu(...)` 的计数值，末尾用空 cpu_name 标记结束。

**用户态 CpuSoftIrqMonitor**：

- open/mmap `/dev/cpu_softirq_monitor`，按 `struct softirq_stat` 解析。
- 对每个 CPU：若存在上次采样 `_cpu_softirqs[cpu_name]`，则用 (当前值 - 上次值) / 时间差(秒) 得到每秒速率，写入 `add_soft_irq()` 的各字段；否则首次只写原始累计值。
- 更新缓存与时间戳供下次差分。

#### 3.3.3 共同设计点

- **内核与用户态结构体一致**：`monitor_struct.h` 中与 kmod 内定义保持同一布局，避免解析错误。
- **mmap 只读、非缓存**：用户态只读，内核定时更新；非缓存保证读到的是定时器刚写入的值。
- **定时器 1 秒**：与常见监控周期一致，用户态差分时时间间隔足够大，避免除零或抖动。
- **设备未就绪**：用户态若 open 失败（ENOENT 等），可标记“设备缺失”并降级（如不填该部分或回退 /proc），与 eBPF 加载失败时的处理思路一致。
