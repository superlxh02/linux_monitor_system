/*
 * cpu_stat_collector.c - CPU 状态统计数据采集内核模块
 *
 * 功能：
 * 1. 在内核空间分配结构体数组内存，存放所有 CPU 的状态统计数据
 * 2. 使用高精度定时器每秒从 per_cpu(kstat_cpu, cpu).cpustat[] 读取数据并更新
 * 3. 注册字符设备 /dev/cpu_stat_monitor，通过 mmap 暴露给用户空间
 *
 * 数据来源：
 * - Linux 内核中每个 CPU 都有一个 struct kernel_cpustat 类型的 per-CPU 变量
 * - 该结构体中的 cpustat[] 数组记录 CPU 在不同状态下的累计时间
 * - 状态包括：user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice
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
#include <linux/kernel_stat.h>
#include <linux/cpumask.h>
#include <linux/version.h>
#include <linux/tick.h>
#include <asm/io.h>

#define DEVICE_NAME "cpu_stat_monitor"
#define CLASS_NAME  "cpu_stat_monitor"
#define MAX_CPUS    256

/* 将纳秒转换为 jiffies（clock_t）- 内联实现 */
static inline u64 nsec_to_jiffies(u64 nsec)
{
    return div_u64(nsec, NSEC_PER_SEC / HZ);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Monitor System");
MODULE_DESCRIPTION("CPU statistics collector via mmap");
MODULE_VERSION("1.0");

/* CPU 状态统计结构体 - 与用户空间共享 */
struct cpu_stat {
    char cpu_name[16];      /* CPU 名称，如 "cpu0" */
    uint64_t user;          /* 用户态时间 */
    uint64_t nice;          /* 低优先级用户态时间 */
    uint64_t system;        /* 内核态时间 */
    uint64_t idle;          /* 空闲时间 */
    uint64_t iowait;        /* I/O 等待时间 */
    uint64_t irq;           /* 硬中断时间 */
    uint64_t softirq;       /* 软中断时间 */
    uint64_t steal;         /* 虚拟化偷取时间 */
    uint64_t guest;         /* 虚拟机运行时间 */
    uint64_t guest_nice;    /* 低优先级虚拟机运行时间 */
};

/* 全局变量 */
static dev_t dev_num;
static struct cdev cpu_stat_cdev;
static struct class *cpu_stat_class;
static struct device *cpu_stat_device;

static struct cpu_stat *cpu_stat_data;      /* 共享内存区域 */
static unsigned long data_size;              /* 数据区大小 */
static int num_cpus;                         /* CPU 数量 */

static struct hrtimer update_timer;          /* 高精度定时器 */
static ktime_t timer_interval;               /* 定时器间隔 */

/*
 * 获取 CPU 空闲时间
 * 需要考虑 NO_HZ 模式下的空闲时间统计
 */
static u64 cpu_stat_get_idle_time(int cpu)
{
    u64 idle_time = -1ULL;

    /* 尝试获取精确的空闲时间（考虑 tickless 模式） */
    idle_time = get_cpu_idle_time_us(cpu, NULL);

    if (idle_time == -1ULL) {
        /* 回退到传统方式 */
        idle_time = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
        idle_time = nsec_to_jiffies(idle_time);
    } else {
        /* 转换微秒到 clock_t */
        idle_time = usecs_to_jiffies(idle_time);
    }

    return idle_time;
}

/*
 * 获取 CPU I/O 等待时间
 * 需要考虑 NO_HZ 模式下的 iowait 时间统计
 */
static u64 cpu_stat_get_iowait_time(int cpu)
{
    u64 iowait_time = -1ULL;

    /* 尝试获取精确的 iowait 时间（考虑 tickless 模式） */
    iowait_time = get_cpu_iowait_time_us(cpu, NULL);

    if (iowait_time == -1ULL) {
        /* 回退到传统方式 */
        iowait_time = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
        iowait_time = nsec_to_jiffies(iowait_time);
    } else {
        /* 转换微秒到 clock_t */
        iowait_time = usecs_to_jiffies(iowait_time);
    }

    return iowait_time;
}

/*
 * 更新 CPU 状态统计数据
 * 从内核的 kcpustat_cpu 读取数据并填充到共享内存
 */
static void update_cpu_stats(void)
{
    int cpu;
    int idx = 0;
    struct kernel_cpustat *kcs;

    for_each_possible_cpu(cpu) {
        if (idx >= MAX_CPUS)
            break;

        kcs = &kcpustat_cpu(cpu);

        snprintf(cpu_stat_data[idx].cpu_name, 
                 sizeof(cpu_stat_data[idx].cpu_name), 
                 "cpu%d", cpu);

        /* 读取各类 CPU 时间统计（单位：纳秒转换为 jiffies） */
        cpu_stat_data[idx].user = nsec_to_jiffies(kcs->cpustat[CPUTIME_USER]);
        cpu_stat_data[idx].nice = nsec_to_jiffies(kcs->cpustat[CPUTIME_NICE]);
        cpu_stat_data[idx].system = nsec_to_jiffies(kcs->cpustat[CPUTIME_SYSTEM]);
        cpu_stat_data[idx].idle = cpu_stat_get_idle_time(cpu);
        cpu_stat_data[idx].iowait = cpu_stat_get_iowait_time(cpu);
        cpu_stat_data[idx].irq = nsec_to_jiffies(kcs->cpustat[CPUTIME_IRQ]);
        cpu_stat_data[idx].softirq = nsec_to_jiffies(kcs->cpustat[CPUTIME_SOFTIRQ]);
        cpu_stat_data[idx].steal = nsec_to_jiffies(kcs->cpustat[CPUTIME_STEAL]);
        cpu_stat_data[idx].guest = nsec_to_jiffies(kcs->cpustat[CPUTIME_GUEST]);
        cpu_stat_data[idx].guest_nice = nsec_to_jiffies(kcs->cpustat[CPUTIME_GUEST_NICE]);

        idx++;
    }

    /* 标记结束 */
    if (idx < MAX_CPUS) {
        cpu_stat_data[idx].cpu_name[0] = '\0';
    }
}

/*
 * 高精度定时器回调函数
 * 每秒触发一次，更新 CPU 状态统计数据
 */
static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    update_cpu_stats();
    
    /* 重新启动定时器 */
    hrtimer_forward_now(timer, timer_interval);
    return HRTIMER_RESTART;
}

/*
 * 设备打开回调
 */
static int cpu_stat_open(struct inode *inode, struct file *file)
{
    pr_info("%s: device opened\n", DEVICE_NAME);
    return 0;
}

/*
 * 设备关闭回调
 */
static int cpu_stat_release(struct inode *inode, struct file *file)
{
    pr_info("%s: device closed\n", DEVICE_NAME);
    return 0;
}

/*
 * mmap 回调 - 将内核数据映射到用户空间
 */
static int cpu_stat_mmap(struct file *file, struct vm_area_struct *vma)
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

    /* 获取物理页帧号 */
    pfn = page_to_pfn(virt_to_page(cpu_stat_data));

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
static const struct file_operations cpu_stat_fops = {
    .owner   = THIS_MODULE,
    .open    = cpu_stat_open,
    .release = cpu_stat_release,
    .mmap    = cpu_stat_mmap,
};


/*
 * 模块初始化
 */
static int __init cpu_stat_collector_init(void)
{
    int ret;

    pr_info("%s: initializing module\n", DEVICE_NAME);

    /* 获取 CPU 数量 */
    num_cpus = num_possible_cpus();
    if (num_cpus > MAX_CPUS)
        num_cpus = MAX_CPUS;

    pr_info("%s: detected %d CPUs\n", DEVICE_NAME, num_cpus);

    /* 分配共享内存 - 使用 PAGE_SIZE 对齐以支持 mmap */
    data_size = PAGE_ALIGN(sizeof(struct cpu_stat) * MAX_CPUS);
    cpu_stat_data = (struct cpu_stat *)__get_free_pages(GFP_KERNEL | __GFP_ZERO,
                                                        get_order(data_size));
    if (!cpu_stat_data) {
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
    cdev_init(&cpu_stat_cdev, &cpu_stat_fops);
    cpu_stat_cdev.owner = THIS_MODULE;

    ret = cdev_add(&cpu_stat_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("%s: failed to add cdev\n", DEVICE_NAME);
        goto err_unregister;
    }

    /* 创建设备类 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cpu_stat_class = class_create(CLASS_NAME);
#else
    cpu_stat_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(cpu_stat_class)) {
        pr_err("%s: failed to create class\n", DEVICE_NAME);
        ret = PTR_ERR(cpu_stat_class);
        goto err_cdev_del;
    }

    /* 创建设备节点 */
    cpu_stat_device = device_create(cpu_stat_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(cpu_stat_device)) {
        pr_err("%s: failed to create device\n", DEVICE_NAME);
        ret = PTR_ERR(cpu_stat_device);
        goto err_class_destroy;
    }

    /* 初始化高精度定时器 */
    timer_interval = ktime_set(1, 0);  /* 1 秒 */
    hrtimer_init(&update_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    update_timer.function = timer_callback;

    /* 首次更新数据 */
    update_cpu_stats();

    /* 启动定时器 */
    hrtimer_start(&update_timer, timer_interval, HRTIMER_MODE_REL);

    pr_info("%s: module loaded successfully\n", DEVICE_NAME);
    return 0;

err_class_destroy:
    class_destroy(cpu_stat_class);
err_cdev_del:
    cdev_del(&cpu_stat_cdev);
err_unregister:
    unregister_chrdev_region(dev_num, 1);
err_free_mem:
    free_pages((unsigned long)cpu_stat_data, get_order(data_size));
    return ret;
}

/*
 * 模块卸载
 */
static void __exit cpu_stat_collector_exit(void)
{
    pr_info("%s: unloading module\n", DEVICE_NAME);

    /* 停止定时器 */
    hrtimer_cancel(&update_timer);

    /* 销毁设备 */
    device_destroy(cpu_stat_class, dev_num);
    class_destroy(cpu_stat_class);
    cdev_del(&cpu_stat_cdev);
    unregister_chrdev_region(dev_num, 1);

    /* 释放内存 */
    free_pages((unsigned long)cpu_stat_data, get_order(data_size));

    pr_info("%s: module unloaded\n", DEVICE_NAME);
}

module_init(cpu_stat_collector_init);
module_exit(cpu_stat_collector_exit);
