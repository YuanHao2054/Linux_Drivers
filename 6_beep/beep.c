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

#define BEEP_CNT 1          /*设备号个数*/
#define BEEP_NAME "beep" /*设备名*/
#define BEEPOFF 0               /*关蜂鸣器*/
#define BEEPON 1                /*开蜂鸣器*/

/*beep设备结构体*/
struct beep_dev
{
    dev_t devid;            /*设备号*/
    struct cdev cdev;       /*cdev*/
    struct class *class;    /*类*/
    struct device *device;  /*设备*/
    int major;              /*主设备号*/
    int minor;              /*次设备号*/
    struct device_node *nd; /*设备节点*/
    int beep_gpio;           /*beep所使用的GPIO编号*/
};

struct beep_dev beep; /*beep设备*/

/*打开设备*/
static int beep_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &beep; /*设置私有数据*/
    return 0;
}

/*从设备读取数据*/
static ssize_t beep_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*向设备写数据*/
static ssize_t beep_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char beepstat;

    /*通过读取文件的私有数据得到设备结构体变量*/
    struct beep_dev *dev = filp->private_data;

    /*获取从用户空间得到的信息*/
    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    /*获取状态值*/
    beepstat = databuf[0];
    if (beepstat == BEEPON)
    {
        /*直接通过调用gpio_set_value来向gpio写入数据，来实现开关灯的效果*/
        gpio_set_value(dev->beep_gpio, 0); /*打开beep灯*/
    }
    else if (beepstat == BEEPOFF)
    {
        gpio_set_value(dev->beep_gpio, 1); /*关闭beep灯*/
    }

    return 0;
}

/*关闭/释放设备*/
static int beep_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*设备操作函数*/
static struct file_operations beep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open,
    .read = beep_read,
    .write = beep_write,
    .release = beep_release,
};

/*驱动入口函数*/
static int __init beep_init(void)
{
    int ret = 0;

    /*设置beep所使用的GPIO*/

    /*1、获取设备节点*/
    beep.nd = of_find_node_by_path("/beep");
    if (beep.nd == NULL)
    {
        printk("beep node can not found!\r\n");
        return -EINVAL;
    }
    else
    {
        printk("beep node has been found!\r\n");
    }

    /*2、获取设备树中的gpio属性，得到beep的GPIO编号*/
    beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpio", 0);
    if (beep.beep_gpio < 0)
    {
        printk("can't get beep-gpio");
        return -EINVAL;
    }
    printk("beep-gpio num = %d\r\n", beep.beep_gpio);

    /*3、设置GPIO5_IO01为输出，并且输出高电平，默认关闭beep灯*/
    ret = gpio_direction_output(beep.beep_gpio, 1);
    if (ret < 0)
    {
        printk("can't set gpio!\r\n");
    }

    /*注册字符设备驱动*/
    /*1、创建设备号*/
    if (beep.major) /*指定了设备号*/
    {
        beep.devid = MKDEV(beep.major, 0);
        register_chrdev_region(beep.devid, BEEP_CNT, BEEP_CNT);
    }
    else
    {
        alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    printk("beep major = %d, minor = %d\r\n", beep.major, beep.minor);

    /*2、初始化cdev*/
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev, &beep_fops);

    /*3、添加一个cdev*/
    cdev_add(&beep.cdev, beep.devid, BEEP_CNT);

    /*4、创建类*/
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if (IS_ERR(beep.class))
    {
        return PTR_ERR(beep.class);
    }

    /*4、创建设备*/
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);

    if (IS_ERR(beep.device))
    {
        return PTR_ERR(beep.device);
    }

    return 0;
}

/*驱动出口函数*/
static void __exit beep_exit(void)
{
    /*注销字符设备驱动*/
    cdev_del(&beep.cdev); /*删除cdev*/
    unregister_chrdev_region(beep.devid, BEEP_CNT);

    device_destroy(beep.class, beep.devid);
    class_destroy(beep.class);
}

module_init(beep_init);
module_exit(beep_exit);

/*驱动信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");