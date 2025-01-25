#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>



/*主设备号和名字*/
#define LED_MAJOR 200
#define LED_NAME "led"

/*led操作*/
#define LED_OFF 0
#define LED_ON 1

/*寄存器物理地址*/
#define CCM_CCGR1_BASE (0x020C406C)         // 时钟使能寄存器
#define SW_MUX_GPIO1_IO03_BASE (0x020E0068) // 复用寄存器
#define SW_PAD_GPIO1_IO03_BASE (0x020E02F4) // 配置寄存器
#define GPIO1_DR_BASE (0x0209C000)          // 数据寄存器，控制输出，该寄存器的第三位控制LED灯
#define GPIO1_GDIR_BASE (0x0209C004)        // 配置功能

/*映射后的寄存器虚拟地址指针*/
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR; 
static void __iomem *GPIO1_GDIR;

/*控制LED灯*/
void led_switch(uint8_t sta)
{
    uint32_t val = 0;
    if (sta == LED_ON)
    {
        /*读取数据寄存器的值*/
        val = readl(GPIO1_DR);
        /*1 << 3，就是将1左移3位，得到0000 1000，~(1 << 3)，即为1111 0111*/
        val &= ~(1 << 3); //val &= 1111 0111，按位与操作，将第3位置0
        /*将数据写入数据寄存器*/
        writel(val, GPIO1_DR);
    }
    else if (sta == LED_OFF)
    {
        val = readl(GPIO1_DR);
        val |= (1 << 3); //val |= 0000 1000，按位或操作，将第3位置1
        writel(val, GPIO1_DR);
    }
}

/*打开设备*/
static int led_open(struct inode *inode, struct file *filp)
{
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

    retvalue = copy_from_user(databuf, buf, cnt);
    if (retvalue < 0)
    {
        printk("kernel write failed!\r\n");
        return -EFAULT;
    }

    /*从用户空间获取状态0或1，通过led_switch操作寄存器*/
    ledstat = databuf[0];
    if (ledstat == LED_ON)
    {
        led_switch(LED_ON); /*打开led*/
        printk("open led!\r\n");
    }
    else if (ledstat == LED_OFF)
    {
        led_switch(LED_OFF);/*关闭led*/
        printk("close led!\r\n");
    }

    return 0;
}

/*关闭/释放设备*/
static int led_release(struct inode *inode, struct file *filp)
{
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
    int retvalue;

    uint32_t val = 0;

    /*初始化LED灯*/
    /*1. 寄存器地址映射*/
    IMX6U_CCM_CCGR1 = ioremap(CCM_CCGR1_BASE, 4); // 时钟使能寄存器
    SW_MUX_GPIO1_IO03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4); // 复用寄存器
    SW_PAD_GPIO1_IO03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4); // 配置寄存器
    GPIO1_DR = ioremap(GPIO1_DR_BASE, 4); // 数据寄存器
    GPIO1_GDIR = ioremap(GPIO1_GDIR_BASE, 4); // 配置功能

    /*2. 使能GPIO1时钟*/
    val = readl(IMX6U_CCM_CCGR1);
    /*该寄存器的bit26、27，将3（11）左移26位，来操作寄存器的26、27位*/
    val &= ~(3 << 26); //清零
    val |= (3 << 26);  //设置 bit26、27都设置位1
    writel(val, IMX6U_CCM_CCGR1);

    /*3. 设置复用功能，将GPIO1_IO03设置为GPIO1_IO03*/
    writel(5, SW_MUX_GPIO1_IO03);

    /*寄存器SW_PAD_GPIO1_IO03设置IO属性*/
    writel(0x10B0, SW_PAD_GPIO1_IO03);

    /*4. 设置GPIO1_IO03为输出功能，bit3要置1*/
    val = readl(GPIO1_GDIR);
    val &= ~(1 << 3); //清零
    val |= (1 << 3);  //设置
    writel(val, GPIO1_GDIR);

    /*5. 默认关闭LED灯*/
    val = readl(GPIO1_DR);
    val |= (1 << 3); //设置 bit3 置1 默认关闭led
    writel(val, GPIO1_DR);

    /*6. 注册字符设备驱动*/
    retvalue = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);

    if (retvalue < 0)
    {
        printk("register led driver failed!\r\n");
        return -EIO;
    }
    else
    {
        printk("register led driver succeeded!\r\n");
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

    /*注销字符设备驱动*/
    unregister_chrdev(LED_MAJOR, LED_NAME);
    printk("unregister led driver succeeded!\r\n");
}

/*模块注册*/
module_init(led_init);
module_exit(led_exit);

/*驱动信息*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");