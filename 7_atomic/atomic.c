#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define GPIOLED_CNT 1          /*设备号个数*/
#define GPIOLED_NAME "gpioled" /*设备名*/
#define LEDOFF 0               /*关灯*/
#define LEDON 1                /*开灯*/

/*gpioled设备结构体*/
struct gpioled_dev
{
    dev_t devid;            /*设备号*/
    struct cdev cdev;       /*cdev*/
    struct class *class;    /*类*/
    struct device *device;  /*设备*/
    int major;              /*主设备号*/
    int minor;              /*次设备号*/
    struct device_node *nd; /*设备节点*/
    int led_gpio;           /*led所使用的GPIO编号*/
    atomic_t lock;          /*原子变量*/
};

struct gpioled_dev gpioled; /*led设备*/

/*打开设备*/
static int led_open(struct inode *inode, struct file *filp)
{
    /*通过判断原子变量的值来检查LED有没有被别的应用使用*/
    /*
    gpioled.lock的值为1，表示LED没有被使用
    atomic_dec_and_test(&gpioled.lock)表示给gpioled.lock减1，如果减1后的值为0，则返回true，否则返回false
    如果已被使用，那么给gpioled.lock加1，恢复成0，同时返回EBUSY，表示设备忙
    */
    if (!atomic_dec_and_test(&gpioled.lock)) 
    {
        atomic_inc(&gpioled.lock); /*小于0的话就+1，使原子变量等于0*/
        printk("device is used by other app, open failed!\r\n");
        return -EBUSY; /*LED被使用，返回忙*/
    }
    filp->private_data = &gpioled; /*设置私有数据*/
    return 0;
}

/*从设备读取数据*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*向设备写数据*/
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;

    /*通过读取文件的私有数据得到设备结构体变量*/
    struct gpioled_dev *dev = filp->private_data;

    /*获取从用户空间得到的信息*/
    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    /*获取状态值*/
    ledstat = databuf[0];
    if (ledstat == LEDON)
    {
        /*直接通过调用gpio_set_value来向gpio写入数据，来实现开关灯的效果*/
        gpio_set_value(dev->led_gpio, 0); /*打开led灯*/
    }
    else if (ledstat == LEDOFF)
    {
        gpio_set_value(dev->led_gpio, 1); /*关闭led灯*/
    }

    return 0;
}

/*关闭/释放设备*/
static int led_release(struct inode *inode, struct file *filp)
{
    struct gpioled_dev *dev = filp->private_data;
    /*关闭驱动文件时释放原子变量，也就是将lock加1*/
    atomic_inc(&dev->lock); 
    return 0;
}

/*设备操作函数*/
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/*驱动入口函数*/
static int __init led_init(void)
{
    int ret = 0;
    
    /*初始化原子变量*/
    atomic_set(&gpioled.lock, 1); /*初始化原子变量，初值为1*/

    /*设置LED所使用的GPIO*/

    /*1、获取设备节点*/
    gpioled.nd = of_find_node_by_path("/gpioled");
    if (gpioled.nd == NULL)
    {
        printk("gpioled node can not found!\r\n");
        return -EINVAL;
    }
    else
    {
        printk("gpioled node has been found!\r\n");
    }

    /*2、获取设备树中的gpio属性，得到LED的GPIO编号*/
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpio", 0);
    if (gpioled.led_gpio < 0)
    {
        printk("can't get led-gpio");
        return -EINVAL;
    }
    printk("led-gpio num = %d\r\n", gpioled.led_gpio);

    /*3、设置GPIO1_IO03为输出，并且输出高电平，默认关闭led灯*/
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if (ret < 0)
    {
        printk("can't set gpio!\r\n");
    }

    /*注册字符设备驱动*/
    /*1、创建设备号*/
    if (gpioled.major) /*指定了设备号*/
    {
        gpioled.devid = MKDEV(gpioled.major, 0);
        register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
    }
    else
    {
        alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    printk("gpioled major = %d, minor = %d\r\n", gpioled.major, gpioled.minor);

    /*2、初始化cdev*/
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &led_fops);

    /*3、添加一个cdev*/
    cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);

    /*4、创建类*/
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class))
    {
        return PTR_ERR(gpioled.class);
    }

    /*4、创建设备*/
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);

    if (IS_ERR(gpioled.device))
    {
        return PTR_ERR(gpioled.device);
    }

    return 0;
}

/*驱动出口函数*/
static void __exit led_exit(void)
{
    /*注销字符设备驱动*/
    cdev_del(&gpioled.cdev); /*删除cdev*/
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);

    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);
}

module_init(led_init);
module_exit(led_exit);

/*驱动信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");