-- Monitor System Database Schema
-- 用于存储服务器性能监控数据

CREATE DATABASE IF NOT EXISTS monitor_db DEFAULT CHARACTER SET utf8mb4;
USE monitor_db;

-- 1. 服务器性能汇总表（主表）
CREATE TABLE IF NOT EXISTS server_performance (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    -- CPU 指标
    cpu_percent FLOAT DEFAULT 0,              -- CPU总使用率，单位：%
    usr_percent FLOAT DEFAULT 0,              -- 用户态CPU使用率，单位：%
    system_percent FLOAT DEFAULT 0,           -- 内核态CPU使用率，单位：%
    nice_percent FLOAT DEFAULT 0,             -- 低优先级用户态CPU使用率，单位：%
    idle_percent FLOAT DEFAULT 0,             -- 空闲时间占比，单位：%
    io_wait_percent FLOAT DEFAULT 0,          -- IO等待时间占比，单位：%
    irq_percent FLOAT DEFAULT 0,              -- 硬中断时间百分比，单位：%
    soft_irq_percent FLOAT DEFAULT 0,         -- 软中断时间百分比，单位：%
    -- 负载指标
    load_avg_1 FLOAT DEFAULT 0,               -- 1分钟负载
    load_avg_3 FLOAT DEFAULT 0,               -- 3分钟负载
    load_avg_15 FLOAT DEFAULT 0,              -- 15分钟负载
    -- 内存指标
    mem_used_percent FLOAT DEFAULT 0,         -- 内存使用率，单位：%
    total FLOAT DEFAULT 0,                    -- 总内存，单位：GB
    free FLOAT DEFAULT 0,                     -- 空闲内存，单位：GB
    avail FLOAT DEFAULT 0,                    -- 可用内存，单位：GB
    -- 磁盘指标
    disk_util_percent FLOAT DEFAULT 0,        -- 磁盘利用率（最大值），单位：%
    -- 网络指标
    send_rate FLOAT DEFAULT 0,                -- 发送速率，单位：kB/s
    rcv_rate FLOAT DEFAULT 0,                 -- 接收速率，单位：kB/s
    -- 性能评分
    score FLOAT DEFAULT 0,                    -- 综合评分，范围：0-100，分数越高性能越好
    -- CPU 变化率
    cpu_percent_rate FLOAT DEFAULT 0,         -- CPU总使用率变化率
    usr_percent_rate FLOAT DEFAULT 0,         -- 用户态CPU变化率
    system_percent_rate FLOAT DEFAULT 0,      -- 内核态CPU变化率
    nice_percent_rate FLOAT DEFAULT 0,        -- 低优先级用户态CPU变化率
    idle_percent_rate FLOAT DEFAULT 0,        -- 空闲CPU变化率
    io_wait_percent_rate FLOAT DEFAULT 0,     -- IO等待变化率
    irq_percent_rate FLOAT DEFAULT 0,         -- 硬中断变化率
    soft_irq_percent_rate FLOAT DEFAULT 0,    -- 软中断变化率
    -- 负载变化率
    load_avg_1_rate FLOAT DEFAULT 0,          -- 1分钟负载变化率
    load_avg_3_rate FLOAT DEFAULT 0,          -- 3分钟负载变化率
    load_avg_15_rate FLOAT DEFAULT 0,         -- 15分钟负载变化率
    -- 内存变化率
    mem_used_percent_rate FLOAT DEFAULT 0,    -- 内存使用率变化率
    total_rate FLOAT DEFAULT 0,               -- 总内存变化率
    free_rate FLOAT DEFAULT 0,                -- 空闲内存变化率
    avail_rate FLOAT DEFAULT 0,               -- 可用内存变化率
    -- 磁盘变化率
    disk_util_percent_rate FLOAT DEFAULT 0,   -- 磁盘利用率变化率
    -- 网络变化率
    send_rate_rate FLOAT DEFAULT 0,           -- 发送速率变化率
    rcv_rate_rate FLOAT DEFAULT 0,            -- 接收速率变化率
    -- 时间戳
    timestamp DATETIME NOT NULL,              -- 采集时间
    INDEX idx_server_time(server_name, timestamp),
    INDEX idx_score(score)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 2. 网络详细数据表
CREATE TABLE IF NOT EXISTS server_net_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    net_name VARCHAR(64) NOT NULL,              -- 网卡名称，如 eth0, ens33
    -- 错误和丢弃统计
    err_in BIGINT DEFAULT 0,                    -- 接收错误数
    err_out BIGINT DEFAULT 0,                   -- 发送错误数
    drop_in BIGINT DEFAULT 0,                   -- 接收丢弃数
    drop_out BIGINT DEFAULT 0,                  -- 发送丢弃数
    -- 速率指标
    rcv_bytes_rate FLOAT DEFAULT 0,             -- 接收速率，单位：kB/s
    rcv_packets_rate FLOAT DEFAULT 0,           -- 接收包数速率，单位：个/s
    snd_bytes_rate FLOAT DEFAULT 0,             -- 发送速率，单位：kB/s
    snd_packets_rate FLOAT DEFAULT 0,           -- 发送包数速率，单位：个/s
    -- 速率变化率
    rcv_bytes_rate_rate FLOAT DEFAULT 0,        -- 接收速率变化率
    rcv_packets_rate_rate FLOAT DEFAULT 0,      -- 接收包数速率变化率
    snd_bytes_rate_rate FLOAT DEFAULT 0,        -- 发送速率变化率
    snd_packets_rate_rate FLOAT DEFAULT 0,      -- 发送包数速率变化率
    -- 错误和丢弃变化率
    err_in_rate FLOAT DEFAULT 0,                -- 接收错误数变化率
    err_out_rate FLOAT DEFAULT 0,               -- 发送错误数变化率
    drop_in_rate FLOAT DEFAULT 0,               -- 接收丢弃数变化率
    drop_out_rate FLOAT DEFAULT 0,              -- 发送丢弃数变化率
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_net_time(server_name, net_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 3. 软中断详细数据表
CREATE TABLE IF NOT EXISTS server_softirq_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    cpu_name VARCHAR(64) NOT NULL,
    -- 软中断计数
    hi BIGINT DEFAULT 0,
    timer BIGINT DEFAULT 0,
    net_tx BIGINT DEFAULT 0,
    net_rx BIGINT DEFAULT 0,
    block BIGINT DEFAULT 0,
    irq_poll BIGINT DEFAULT 0,
    tasklet BIGINT DEFAULT 0,
    sched BIGINT DEFAULT 0,
    hrtimer BIGINT DEFAULT 0,
    rcu BIGINT DEFAULT 0,
    -- 变化率
    hi_rate FLOAT DEFAULT 0,
    timer_rate FLOAT DEFAULT 0,
    net_tx_rate FLOAT DEFAULT 0,
    net_rx_rate FLOAT DEFAULT 0,
    block_rate FLOAT DEFAULT 0,
    irq_poll_rate FLOAT DEFAULT 0,
    tasklet_rate FLOAT DEFAULT 0,
    sched_rate FLOAT DEFAULT 0,
    hrtimer_rate FLOAT DEFAULT 0,
    rcu_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_cpu_time(server_name, cpu_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 4. 内存详细数据表
CREATE TABLE IF NOT EXISTS server_mem_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    -- 内存指标
    total FLOAT DEFAULT 0,
    free FLOAT DEFAULT 0,
    avail FLOAT DEFAULT 0,
    buffers FLOAT DEFAULT 0,
    cached FLOAT DEFAULT 0,
    swap_cached FLOAT DEFAULT 0,
    active FLOAT DEFAULT 0,
    inactive FLOAT DEFAULT 0,
    active_anon FLOAT DEFAULT 0,
    inactive_anon FLOAT DEFAULT 0,
    active_file FLOAT DEFAULT 0,
    inactive_file FLOAT DEFAULT 0,
    dirty FLOAT DEFAULT 0,
    writeback FLOAT DEFAULT 0,
    anon_pages FLOAT DEFAULT 0,
    mapped FLOAT DEFAULT 0,
    kreclaimable FLOAT DEFAULT 0,
    sreclaimable FLOAT DEFAULT 0,
    sunreclaim FLOAT DEFAULT 0,
    -- 变化率
    total_rate FLOAT DEFAULT 0,
    free_rate FLOAT DEFAULT 0,
    avail_rate FLOAT DEFAULT 0,
    buffers_rate FLOAT DEFAULT 0,
    cached_rate FLOAT DEFAULT 0,
    swap_cached_rate FLOAT DEFAULT 0,
    active_rate FLOAT DEFAULT 0,
    inactive_rate FLOAT DEFAULT 0,
    active_anon_rate FLOAT DEFAULT 0,
    inactive_anon_rate FLOAT DEFAULT 0,
    active_file_rate FLOAT DEFAULT 0,
    inactive_file_rate FLOAT DEFAULT 0,
    dirty_rate FLOAT DEFAULT 0,
    writeback_rate FLOAT DEFAULT 0,
    anon_pages_rate FLOAT DEFAULT 0,
    mapped_rate FLOAT DEFAULT 0,
    kreclaimable_rate FLOAT DEFAULT 0,
    sreclaimable_rate FLOAT DEFAULT 0,
    sunreclaim_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_time(server_name, timestamp),
    INDEX idx_mem_used(total, free, avail)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- 5. 磁盘详细数据表
CREATE TABLE IF NOT EXISTS server_disk_detail (
    id INT AUTO_INCREMENT PRIMARY KEY,
    server_name VARCHAR(255) NOT NULL,
    disk_name VARCHAR(64) NOT NULL,
    -- 磁盘计数器
    `reads` BIGINT DEFAULT 0,
    `writes` BIGINT DEFAULT 0,
    sectors_read BIGINT DEFAULT 0,
    sectors_written BIGINT DEFAULT 0,
    read_time_ms BIGINT DEFAULT 0,
    write_time_ms BIGINT DEFAULT 0,
    io_in_progress BIGINT DEFAULT 0,
    io_time_ms BIGINT DEFAULT 0,
    weighted_io_time_ms BIGINT DEFAULT 0,
    -- 计算指标
    read_bytes_per_sec FLOAT DEFAULT 0,
    write_bytes_per_sec FLOAT DEFAULT 0,
    read_iops FLOAT DEFAULT 0,
    write_iops FLOAT DEFAULT 0,
    avg_read_latency_ms FLOAT DEFAULT 0,
    avg_write_latency_ms FLOAT DEFAULT 0,
    util_percent FLOAT DEFAULT 0,
    -- 变化率
    read_bytes_per_sec_rate FLOAT DEFAULT 0,
    write_bytes_per_sec_rate FLOAT DEFAULT 0,
    read_iops_rate FLOAT DEFAULT 0,
    write_iops_rate FLOAT DEFAULT 0,
    avg_read_latency_ms_rate FLOAT DEFAULT 0,
    avg_write_latency_ms_rate FLOAT DEFAULT 0,
    util_percent_rate FLOAT DEFAULT 0,
    -- 时间戳
    timestamp DATETIME NOT NULL,
    INDEX idx_server_disk_time(server_name, disk_name, timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
