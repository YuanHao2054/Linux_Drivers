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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/*信息宏定义*/
#define NEWCHRLED_CNT 1              /*设备号个数*/
#define NEWCHRLED_NAME "newchrled"    /*设备名*/
#define LEDOFF 0                      /*关灯*/
#define LEDON  1                      /*开灯*/

/*寄存器物理地址*/
#define CCM_CCGR1_BASE              (0X020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0X020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0X020E02F4)
#define GPIO1_DR_BASE               (0X0209C000)
#define GPIO1_GDIR_BASE             (0X0209C004)


/*映射后的寄存器虚拟地址指针*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;


/*设备信息结构体*/
struct newchrled_dev
{
    dev_t devid;                /*设备号*/
    struct cdev cdev;           /*cdev*/
    struct class *class;        /*类*/
    struct device *device;      /*设备*/
    int major;                  /*主设备号*/
    int minor;                  /*次设备号*/
};

struct newchrled_dev newchrled;

/*led的打开与关闭*/
void led_switch(uint8_t sta)
{
    uint32_t val = 0;
    if(sta == LEDON)
    {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);     //bit3清零，点亮
        writel(val, GPIO1_DR);
    }
    else if(sta == LEDOFF)
    {
        val = readl(GPIO1_DR);
        val |= (1 << 3);      //bit3置1，熄灭
        writel(val, GPIO1_DR);
    }
}

/*打开设备*/
static int newchrled_open(struct inode *inode, struct file *filp)
{
    /*传递私有数据*/
    filp->private_data = &newchrled;    //设置私有数据
    return 0;
}

/*从设备读取数据*/
static ssize_t newchrled_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*向设备写入数据*/
static ssize_t newchrled_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;
    
    /*拷贝从用户空间传递的数据到内核空间*/
    retvalue = copy_from_user(databuf, buf, cnt);
    if(retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];
    if(ledstat == LEDON)
    {
        led_switch(LEDON);
    }
    else if(ledstat == LEDOFF)
    {
        led_switch(LEDOFF);
    }
    return 0;
}

/*关闭/释放设备*/
static int newchrled_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*设备操作函数*/
static struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .open = newchrled_open,
    .read = newchrled_read,
    .write = newchrled_write,
    .release = newchrled_release,
};

/*驱动入口函数*/
static int __init led_init(void)
{
    uint32_t val = 0;

    /*初始化LED*/
    /*1、寄存器地址映射*/
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);

    /*2、使能GPIO1时钟*/
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);      //清除以前的设置
    val |= (3 << 26);       //设置新值
    writel(val, IMX6U_CCM_CCGR1);


    /*3、设置GPIO1_IO03复用功能，将其复用为GPIO1_IO03，最后设置IO属性*/
    writel(5, SW_MUX_GPIO1_IO03);       //设置复用功能
    writel(0x10B0, SW_PAD_GPIO1_IO03);  //设置电气属性
    
    /*4、设置GPIO1_IO03为输出*/
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3);       //bit3清零
    val |= (1 << 3);        //bit3置1
    writel(val, GPIO1_GDIR);

    /*默认关闭LED*/
    val = readl(GPIO1_DR);
    val |= (1 << 3);        //bit3置1
    writel(val, GPIO1_DR);

    /*注册字符设备驱动*/
    if (newchrled.major) //如果已经定义了设备号
    {
        newchrled.devid = MKDEV(newchrled.major, 0);
        register_chrdev_region(newchrled.devid, NEWCHRLED_CNT, NEWCHRLED_NAME);
    }
    else //如果没有定义设备号，就向内核申请一个设备号
    {
        alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_CNT, NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }

    /*打印设备的主次设备号*/
    printk("newchrled major = %d, minor = %d\r\n", newchrled.major, newchrled.minor);

    /*初始化cdev*/
    newchrled.cdev.owner = THIS_MODULE;
    cdev_init(&newchrled.cdev, &newchrled_fops);

    /*添加一个cdev*/
    cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_CNT);

    /*创建类*/
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.class))
    {
        return PTR_ERR(newchrled.class);
    }

    /*创建设备*/
    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);

    if (IS_ERR(newchrled.device))
    {
        return PTR_ERR(newchrled.device);
    }
    return 0;

}

/*驱动出口函数*/
static void __exit led_exit(void)
{
    /*取消映射*/
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    /*删除字符设备*/
    cdev_del(&newchrled.cdev);

    /*注销设备号*/
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_CNT);

    /*摧毁设备*/
    device_destroy(newchrled.class, newchrled.devid);
    /*摧毁类*/
    class_destroy(newchrled.class);

    printk("led_exit\r\n");
}

module_init(led_init);
module_exit(led_exit);

/*添加LICENSE*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");