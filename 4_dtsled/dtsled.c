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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/*信息宏定义*/
#define dtsled_CNT 1         /*设备号个数*/
#define dtsled_NAME "dtsled" /*设备名*/
#define LEDOFF 0                /*关灯*/
#define LEDON 1                 /*开灯*/

/*通过设备树获取物理地址，不需要再驱动中定义*/

// /*寄存器物理地址*/
// #define CCM_CCGR1_BASE (0X020C406C)
// #define SW_MUX_GPIO1_IO03_BASE (0X020E0068)
// #define SW_PAD_GPIO1_IO03_BASE (0X020E02F4)
// #define GPIO1_DR_BASE (0X0209C000)
// #define GPIO1_GDIR_BASE (0X0209C004)

/*映射后的寄存器虚拟地址指针*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

/*设备信息结构体*/
struct dtsled_dev
{
    dev_t devid;           /*设备号*/
    struct cdev cdev;      /*cdev*/
    struct class *class;   /*类*/
    struct device *device; /*设备*/
    int major;             /*主设备号*/
    int minor;             /*次设备号*/

    /*使用OF函数获取设备树节点中的属性值*/
    struct device_node *nd; /*设备树节点*/
};

struct dtsled_dev dtsled; /*led设备*/


/*led的打开与关闭*/
void led_switch(uint8_t sta)
{
    uint32_t val = 0;
    if (sta == LEDON)
    {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3); // bit3清零，点亮
        writel(val, GPIO1_DR);
    }
    else if (sta == LEDOFF)
    {
        val = readl(GPIO1_DR);
        val |= (1 << 3); // bit3置1，熄灭
        writel(val, GPIO1_DR);
    }
}

/*打开设备*/
static int led_open(struct inode *inode, struct file *filp)
{
    /*传递私有数据*/
    filp->private_data = &dtsled; // 设置私有数据
    return 0;
}

/*从设备读取数据*/
static ssize_t led_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    return 0;
}

/*向设备写入数据*/
static ssize_t led_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue;
    unsigned char databuf[1];
    unsigned char ledstat;

    /*拷贝从用户空间传递的数据到内核空间*/
    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    ledstat = databuf[0];
    if (ledstat == LEDON)
    {
        led_switch(LEDON);
    }
    else if (ledstat == LEDOFF)
    {
        led_switch(LEDOFF);
    }
    return 0;
}

/*关闭/释放设备*/
static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

/*设备操作函数*/
static struct file_operations dtsled_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .read = led_read,
    .write = led_write,
    .release = led_release,
};

/*驱动入口函数*/
static int __init led_init(void)
{
    uint32_t val = 0;
    int ret;
    uint32_t regdata[14];

    const char *str;

    /*保存获取到的属性值*/
    struct property *proper;

    /*获取设备树中的属性数据*/

    /*1、获取设备节点：alphaled*/
    dtsled.nd = of_find_node_by_path("/alphaled"); // 通过节点名字查找节点，传递给设备节点指针
    if (dtsled.nd == NULL)
    {
        printk("alphaled node not find!\r\n");
        return -EINVAL;
    }
    else
    {
        printk("alphaled node find!\r\n");
    }

    /*2、获取compatible属性*/
    proper = of_find_property(dtsled.nd, "compatible", NULL); // 通过属性名字查找属性
    if (proper == NULL)
    {
        printk("compatible property find failed!\r\n");
    }
    else
    {
        printk("compatible = %s\r\n", (char *)proper->value);
    }

    /*3、获取status属性*/
    ret = of_property_read_string(dtsled.nd, "status", &str); 
    if (ret < 0)
    {
        printk("status read failed!\r\n");
    }
    else
    {
        printk("status = %s\r\n", str);
    }

    /*4、获取reg属性*/
    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10); 
    if (ret < 0)
    {
        printk("reg property read failed!\r\n");
    }
    else
    {
        uint8_t i = 0;
        printk("reg data:\r\n");
        for (i = 0; i < 10; i++)
        {
            printk("%#X ", regdata[i]);
        }
        printk("\r\n");
    }


    /*寄存器地址映射*/
#if 0

    /*通过of_property_read_u32_array函数获取设备节点的物理地址*/
    IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
    SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
    SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
    GPIO1_DR = ioremap(regdata[6], regdata[7]);
    GPIO1_GDIR = ioremap(regdata[8], regdata[9]);
#else
    /*通过设备节点指针获取设备节点的物理地址*/
    IMX6U_CCM_CCGR1 = of_iomap(dtsled.nd, 0); 
    SW_MUX_GPIO1_IO03 = of_iomap(dtsled.nd, 1);
    SW_PAD_GPIO1_IO03 = of_iomap(dtsled.nd, 2);
    GPIO1_DR = of_iomap(dtsled.nd, 3);
    GPIO1_GDIR = of_iomap(dtsled.nd, 4);
#endif

    /*
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4);
    */
    /*2、使能GPIO1时钟*/
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26); // 清除以前的设置
    val |= (3 << 26);  // 设置新值
    writel(val, IMX6U_CCM_CCGR1);

    /*3、设置GPIO1_IO03复用功能，将其复用为GPIO1_IO03，最后设置IO属性*/
    writel(5, SW_MUX_GPIO1_IO03);      // 设置复用功能
    writel(0x10B0, SW_PAD_GPIO1_IO03); // 设置电气属性

    /*4、设置GPIO1_IO03为输出*/
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3); // bit3清零
    val |= (1 << 3);  // bit3置1
    writel(val, GPIO1_GDIR);

    /*默认关闭LED*/
    val = readl(GPIO1_DR);
    val |= (1 << 3); // bit3置1
    writel(val, GPIO1_DR);

    /*注册字符设备驱动*/
    if (dtsled.major) // 如果已经定义了设备号
    {
        dtsled.devid = MKDEV(dtsled.major, 0);
        register_chrdev_region(dtsled.devid, dtsled_CNT, dtsled_NAME);
    }
    else // 如果没有定义设备号，就向内核申请一个设备号
    {
        alloc_chrdev_region(&dtsled.devid, 0, dtsled_CNT, dtsled_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }

    /*打印设备的主次设备号*/
    printk("dtsled major = %d, minor = %d\r\n", dtsled.major, dtsled.minor);

    /*初始化cdev*/
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

    /*添加一个cdev*/
    cdev_add(&dtsled.cdev, dtsled.devid, dtsled_CNT);

    /*创建类*/
    dtsled.class = class_create(THIS_MODULE, dtsled_NAME);
    if (IS_ERR(dtsled.class))
    {
        return PTR_ERR(dtsled.class);
    }

    /*创建设备*/
    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, dtsled_NAME);

    if (IS_ERR(dtsled.device))
    {
        return PTR_ERR(dtsled.device);
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
    cdev_del(&dtsled.cdev);

    /*注销设备号*/
    unregister_chrdev_region(dtsled.devid, dtsled_CNT);

    /*摧毁设备*/
    device_destroy(dtsled.class, dtsled.devid);
    /*摧毁类*/
    class_destroy(dtsled.class);

    printk("led_exit\r\n");
}

module_init(led_init);
module_exit(led_exit);

/*添加LICENSE*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");