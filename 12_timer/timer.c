#include "asm-generic/gpio.h"
#include "linux/fs.h"
#include "linux/jiffies.h"
#include "linux/spinlock.h"
#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/types.h>

#define TIMER_CNT     1                /*设备号个数*/
#define TIMER_NAME    "timer"          /*设备名*/
#define CLOSE_CMD     (_IO(0xEF, 0x1)) /*关闭定时器命令*/
#define OPEN_CMD      (_IO(0xEF, 0x2)) /*打开定时器命令*/
#define SETPERIOD_CMD (_IO(0XEF, 0X3)) /*设置定时器周期命令*/
#define LEDON         1                /*开灯*/
#define LEDOFF        0                /*关灯*/

/*timer设备结构体*/
struct timer_dev
{
    dev_t devid;             /*设备号*/
    struct cdev cdev;        /*cdev*/
    struct class *class;     /*类*/
    struct device *device;   /*设备*/
    int major;               /*主设备号*/
    int minor;               /*次设备号*/
    struct device_node *nd;  /*设备节点*/
    int led_gpio;            /*led的GPIO编号*/
    int timeperiod;          /*定时周期*/
    struct timer_list timer; /*内核定时器*/
    spinlock_t lock;         /*自旋锁*/
};

/*timer设备*/
struct timer_dev timerdev;

/*初始化led灯，open函数打开驱动时，初始化led灯的GPIO引脚*/
static int led_init(void)
{
    int ret = 0;

    /*1、获取设备节点：timerled*/
    timerdev.nd = of_find_node_by_path("/gpioled");
    if (timerdev.nd == NULL)
    {
        printk("timerled node can not found!\r\n");
        return -EINVAL;
    }
    else
    {
        printk("timerled node has been found!\r\n");
    }

    /*2、获取led的GPIO属性，得到GPIO编号*/
    timerdev.led_gpio = of_get_named_gpio(timerdev.nd, "led-gpio", 0);

    if (timerdev.led_gpio < 0)
    {
        printk("can't get led-gpio");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", timerdev.led_gpio);

    /*3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭led灯*/

    /*请求IO*/
    gpio_request(timerdev.led_gpio, "led");
    ret = gpio_direction_output(timerdev.led_gpio, 1); /*默认关闭led灯*/
    if (ret < 0)
    {
        printk("can't set gpio!\r\n");
    }
    return 0;
}

/*打开设备*/
static int timer_open(struct inode *inode, struct file *filp)
{
    int ret = 0;
    filp->private_data = &timerdev; /*设置私有数据*/

    /*默认周期为1s*/
    timerdev.timeperiod = 1000;

    /*初始化led灯*/
    ret = led_init();
    if (ret < 0)
    {
        return ret;
    }

    /*启动定时器*/
    //mod_timer(&timerdev.timer, jiffies + msecs_to_jiffies(timerdev.timeperiod));

    return 0;
}


/*有bug，接收测试App发送的指令，无法跳转并开启定时器*/
/*ioctl函数*/
static long timer_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)filp->private_data;

    int timerperiod;
    unsigned long flags;

    switch (cmd)
    {
    case CLOSE_CMD: /*关闭定时器*/
        del_timer_sync(&dev->timer);
        break;
    case OPEN_CMD:                            /*打开定时器*/
        spin_lock_irqsave(&dev->lock, flags); /*上锁*/
        timerperiod = dev->timeperiod;
        spin_unlock_irqrestore(&dev->lock, flags); /*解锁*/
        printk("timerperiod = %d\r\n", timerperiod);

        /*设置定时器周期*/
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
        break;

    case SETPERIOD_CMD: /*设置定时器周期*/
        spin_lock_irqsave(&dev->lock, flags);
        dev->timeperiod = arg;
        spin_unlock_irqrestore(&dev->lock, flags);
        printk("set period = %ld\r\n", arg);
        mod_timer(&dev->timer, jiffies + msecs_to_jiffies(arg));
        break;
    default:
        break;
    }
    return 0;
}

/*设备操作函数结构体*/
static struct file_operations timer_fops = {
    .owner = THIS_MODULE,
    .open = timer_open,
    .unlocked_ioctl = timer_unlocked_ioctl,
};

/*定时器回调函数*/
void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1; /*表示led灯的状态*/

    int timerperiod;
    unsigned long flags;

    sta = !sta; /*每次都取反，实现led的反转*/
    gpio_set_value(dev->led_gpio, sta);
    printk("current jiffies is %ld\r\n", jiffies);

    /*重启定时器*/
    spin_lock_irqsave(&dev->lock, flags);
    timerperiod = dev->timeperiod;
    spin_unlock_irqrestore(&dev->lock, flags);
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(timerperiod));
}

/*驱动入口函数*/
static int __init timer_init(void)
{
    /*初始化自旋锁*/
    spin_lock_init(&timerdev.lock);

    /* 注册字符设备驱动 */
    /* 1、创建设备号 */
    if (timerdev.major)
    {
        /* 定义了设备号 */
        timerdev.devid = MKDEV(timerdev.major, 0);
        register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
    }
    else
    { /* 没有定义设备号 */
        alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid); /* 获取主设备号 */
        timerdev.minor = MINOR(timerdev.devid); /* 获取次设备号 */
    }

    printk("timer major = %d,minor = %d\r\n", timerdev.major, timerdev.minor);

    /* 2、初始化 cdev */
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timer_fops);
    /* 3、添加一个 cdev */
    cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
    /* 4、创建类 */
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);

    if (IS_ERR(timerdev.class))
    {
        return PTR_ERR(timerdev.class);
    }

    /* 5、创建设备 */
    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if (IS_ERR(timerdev.device))
    {
        return PTR_ERR(timerdev.device);
    }

    /* 6、初始化 timer，设置定时器处理函数,还未设置周期，所以不会激活定时器 */
    init_timer(&timerdev.timer);
    timerdev.timer.function = timer_function; /*指定定时器回调函数*/
    timerdev.timer.data = (unsigned long)&timerdev;
    return 0;
}

/*驱动出口函数*/
static void __exit timer_exit(void)
{
    /*卸载驱动时关闭led灯*/
    gpio_set_value(timerdev.led_gpio, 1);

    /*删除定时器*/
    del_timer_sync(&timerdev.timer);

    /*注销字符设备驱动*/
    cdev_del(&timerdev.cdev); /*删除 cdev */
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);
    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);
}

module_init(timer_init);
module_exit(timer_exit);

/*添加LICENSE和作者信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");