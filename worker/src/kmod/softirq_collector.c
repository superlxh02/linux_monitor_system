/*
 * softirq_collector.c - 软中断统计数据采集内核模块
 *
 * 功能：
 * 1. 在内核空间分配结构体数组内存，存放所有 CPU 的软中断统计数据
 * 2. 使用高精度定时器每秒从 kstat_softirqs 读取数据并更新
 * 3. 注册字符设备 /dev/cpu_softirq_monitor，通过 mmap 暴露给用户空间
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/version.h>
#include <asm/io.h>

#define DEVICE_NAME "cpu_softirq_monitor"
#define CLASS_NAME  "softirq_monitor"
#define MAX_CPUS    256

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Monitor System");
MODULE_DESCRIPTION("Softirq statistics collector via mmap");
MODULE_VERSION("1.0");

/* 软中断统计结构体 - 与用户空间共享 */
struct softirq_stat {
    char cpu_name[16];      /* CPU 名称，如 "cpu0" */
    uint64_t hi;            /* HI_SOFTIRQ */
    uint64_t timer;         /* TIMER_SOFTIRQ */
    uint64_t net_tx;        /* NET_TX_SOFTIRQ */
    uint64_t net_rx;        /* NET_RX_SOFTIRQ */
    uint64_t block;         /* BLOCK_SOFTIRQ */
    uint64_t irq_poll;      /* IRQ_POLL_SOFTIRQ */
    uint64_t tasklet;       /* TASKLET_SOFTIRQ */
    uint64_t sched;         /* SCHED_SOFTIRQ */
    uint64_t hrtimer;       /* HRTIMER_SOFTIRQ */
    uint64_t rcu;           /* RCU_SOFTIRQ */
};

/* 全局变量 */
static dev_t dev_num;
static struct cdev softirq_cdev;
static struct class *softirq_class;
static struct device *softirq_device;

static struct softirq_stat *softirq_data;   /* 共享内存区域 */
static unsigned long data_size;              /* 数据区大小 */
static int num_cpus;                         /* CPU 数量 */

static struct hrtimer update_timer;          /* 高精度定时器 */
static ktime_t timer_interval;               /* 定时器间隔 */

/*
 * 更新软中断统计数据
 * 从内核的 kstat_softirqs 读取数据并填充到共享内存
 */
static void update_softirq_stats(void)
{
    int cpu;
    int idx = 0;

    for_each_possible_cpu(cpu) {
        if (idx >= MAX_CPUS)
            break;

        snprintf(softirq_data[idx].cpu_name, 
                 sizeof(softirq_data[idx].cpu_name), 
                 "cpu%d", cpu);

        /* 读取各类软中断计数 */
        softirq_data[idx].hi      = kstat_softirqs_cpu(HI_SOFTIRQ, cpu);
        softirq_data[idx].timer   = kstat_softirqs_cpu(TIMER_SOFTIRQ, cpu);
        softirq_data[idx].net_tx  = kstat_softirqs_cpu(NET_TX_SOFTIRQ, cpu);
        softirq_data[idx].net_rx  = kstat_softirqs_cpu(NET_RX_SOFTIRQ, cpu);
        softirq_data[idx].block   = kstat_softirqs_cpu(BLOCK_SOFTIRQ, cpu);
        softirq_data[idx].irq_poll = kstat_softirqs_cpu(IRQ_POLL_SOFTIRQ, cpu);
        softirq_data[idx].tasklet = kstat_softirqs_cpu(TASKLET_SOFTIRQ, cpu);
        softirq_data[idx].sched   = kstat_softirqs_cpu(SCHED_SOFTIRQ, cpu);
        softirq_data[idx].hrtimer = kstat_softirqs_cpu(HRTIMER_SOFTIRQ, cpu);
        softirq_data[idx].rcu     = kstat_softirqs_cpu(RCU_SOFTIRQ, cpu);

        idx++;
    }

    /* 标记结束 */
    if (idx < MAX_CPUS) {
        softirq_data[idx].cpu_name[0] = '\0';
    }
}

/*
 * 高精度定时器回调函数
 * 每秒触发一次，更新软中断统计数据
 */
static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    update_softirq_stats();
    
    /* 重新启动定时器 */
    hrtimer_forward_now(timer, timer_interval);
    return HRTIMER_RESTART;
}

/*
 * 设备打开回调
 */
static int softirq_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

/*
 * 设备关闭回调
 */
static int softirq_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device closed\n", DEVICE_NAME);
    return 0;
}

/*
 * mmap 回调 - 将内核数据映射到用户空间
 */
static int softirq_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    /* 检查映射大小 */
    if (size > data_size) {
        pr_err("%s: mmap size %lu exceeds data size %lu\n", 
               DEVICE_NAME, size, data_size);
        return -EINVAL;
    }

    /* 设置页面属性为不可缓存，确保数据一致性 */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    /* 获取物理页帧号 - 使用 page_to_pfn 替代 virt_to_phys */
    pfn = page_to_pfn(virt_to_page(softirq_data));

    /* 建立映射 */
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("%s: remap_pfn_range failed: %d\n", DEVICE_NAME, ret);
        return ret;
    }

    pr_info("%s: mmap successful, size=%lu\n", DEVICE_NAME, size);
    return 0;
}

/* 文件操作结构体 */
static const struct file_operations softirq_fops = {
    .owner   = THIS_MODULE,
    .open    = softirq_open,
    .release = softirq_release,
    .mmap    = softirq_mmap,
};

/*
 * 模块初始化
 */
static int __init softirq_collector_init(void)
{
    int ret;

    pr_info("%s: initializing module\n", DEVICE_NAME);

    /* 获取 CPU 数量 */
    num_cpus = num_possible_cpus();
    if (num_cpus > MAX_CPUS)
        num_cpus = MAX_CPUS;

    pr_info("%s: detected %d CPUs\n", DEVICE_NAME, num_cpus);

    /* 分配共享内存 - 使用 PAGE_SIZE 对齐以支持 mmap */
    data_size = PAGE_ALIGN(sizeof(struct softirq_stat) * MAX_CPUS);
    softirq_data = (struct softirq_stat *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
                                                           get_order(data_size));
    if (!softirq_data) {
        pr_err("%s: failed to allocate memory\n", DEVICE_NAME);
        return -ENOMEM;
    }

    pr_info("%s: allocated %lu bytes for data\n", DEVICE_NAME, data_size);

    /* 分配设备号 */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("%s: failed to allocate device number\n", DEVICE_NAME);
        goto err_free_mem;
    }

    /* 初始化字符设备 */
    cdev_init(&softirq_cdev, &softirq_fops);
    softirq_cdev.owner = THIS_MODULE;

    ret = cdev_add(&softirq_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("%s: failed to add cdev\n", DEVICE_NAME);
        goto err_unregister;
    }

    /* 创建设备类 - 新版内核 API 只需要一个参数 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    softirq_class = class_create(CLASS_NAME);
#else
    softirq_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(softirq_class)) {
        pr_err("%s: failed to create class\n", DEVICE_NAME);
        ret = PTR_ERR(softirq_class);
        goto err_cdev_del;
    }

    /* 创建设备节点 */
    softirq_device = device_create(softirq_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(softirq_device)) {
        pr_err("%s: failed to create device\n", DEVICE_NAME);
        ret = PTR_ERR(softirq_device);
        goto err_class_destroy;
    }

    /* 初始化高精度定时器 */
    timer_interval = ktime_set(1, 0);  /* 1 秒 */
    hrtimer_init(&update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    update_timer.function = timer_callback;

    /* 首次更新数据 */
    update_softirq_stats();

    /* 启动定时器 */
    hrtimer_start(&update_timer, timer_interval, HRTIMER_MODE_REL);

    pr_info("%s: module loaded successfully\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(softirq_class);
err_cdev_del:
    cdev_del(&softirq_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_mem:
    free_pages((unsigned long)softirq_data, get_order(data_size));
    return ret;
}

/*
 * 模块卸载
 */
static void __exit softirq_collector_exit(void)
{
    pr_info("%s: unloading module\n", DEVICE_NAME);

    /* 停止定时器 */
    hrtimer_cancel(&update_timer);

    /* 销毁设备 */
    device_destroy(softirq_class, dev_num);
    class_destroy(softirq_class);
    cdev_del(&softirq_cdev);
    unregister_chrdev_region(dev_num, 1);

    /* 释放内存 */
    free_pages((unsigned long)softirq_data, get_order(data_size));

    pr_info("%s: module unloaded\n", DEVICE_NAME);
}

module_init(softirq_collector_init);
module_exit(softirq_collector_exit);
