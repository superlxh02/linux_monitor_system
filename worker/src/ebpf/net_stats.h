/*
 * net_stats.h - eBPF 网络统计共享数据结构
 * 
 * 此头文件定义了 eBPF 程序和用户空间程序共享的数据结构
 */

#ifndef __NET_STATS_H__
#define __NET_STATS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 网络统计结构体 - 与 eBPF 程序共享 */
struct net_stats {
    uint64_t rcv_bytes;      /* 接收字节数 */
    uint64_t rcv_packets;    /* 接收包数 */
    uint64_t snd_bytes;      /* 发送字节数 */
    uint64_t snd_packets;    /* 发送包数 */
};

/* Map 名称 */
#define NET_STATS_MAP_NAME "net_stats_map"

/* 最大网卡数量 */
#define MAX_NET_DEVICES 64

#ifdef __cplusplus
}
#endif

#endif /* __NET_STATS_H__ */
