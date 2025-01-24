#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>

/*该设备运行在内核空间，会有用户空间的程序对这个设备进行访问，和用户空间通信的缓冲区:形参中的char __user *buf */

/*主设备号和名称*/
#define CHRDEVBASE_MAJOR 200
#define CHRDEVBASE_NAME "chardevbase"

/*读缓冲区和写缓存区*/
static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = "kernel data";


/*
描述：打开设备
参数：inode：传递给驱动的inode结构体指针
      filp：设备文件，file结构体指针
return：0成功；其他失败
*/
static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    //printk("chardevbase open\n");
    return 0;
}

/*
描述：从设备读取数据
参数: filp: 要打开的设备文件
      buf: 返回给用户空间的数据缓冲区
      cnt: 要读取的数据长度
      offt: 相对于文件首地址的偏移
return: 读取的字节数，如果为负数，表示读取失败
*/
static ssize_t chrdevbase_read(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;

    /*向用户空间发送数据*/
    //先将内核空间的数据拷贝到readbuf中
    memcpy(readbuf, kerneldata, sizeof(kerneldata));

    //内核空间不能直接操作用户空间的内存，需要使用copy_to_user函数，将readbuf中的数据拷贝到buf中
    retvalue = copy_to_user(buf, readbuf, cnt);

    if (retvalue == 0)
    {
        printk("kernel send data to user space:%s\n", writebuf);
    }
    else
    {
        printk("kernel send data to user space failed\n");
    }
    return 0;
}

/*
描述：向设备写入数据
参数: filp: 要打开的设备文件
      buf: 要写入的数据缓冲区
      cnt: 要写入的数据长度
      offt: 相对于文件首地址的偏移
return: 写入的字节数，如果为负数，表示写入失败
*/
static ssize_t chrdevbase_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    int retvalue = 0;

    /*接收用户空间传递给内核的数据并且打印出来*/
    //将buf中的数据拷贝到writebuf中
    retvalue = copy_from_user(writebuf, buf, cnt);
    if (retvalue == 0)
    {
        printk("kernel recv data from user space:%s\n", writebuf);
    }
    else
    {
        printk("kernel recv data from user space failed\n");
    }
    return 0;
}

/*
描述：关闭/释放设备
参数：inode：传递给驱动的inode结构体指针
      filp：设备文件，file结构体指针
return：0成功；其他失败
*/
static int chrdevbase_release(struct inode *inode, struct file *filp)
{
    //printk("chardevbase release\n");
    return 0;
}

/*文件操作结构体*/
static struct file_operations chardevbase_fops = 
{
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .read = chrdevbase_read,
    .write = chrdevbase_write,
    .release = chrdevbase_release,
};

/*
描述：驱动入口函数
参数：无
return：0成功；其他失败
*/
static int __init chrdevbase_init(void)
{
    int retvalue = 0;

    /*注册字符设备驱动*/
    retvalue = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chardevbase_fops);

    if (retvalue < 0)
    {
        printk("chardevbase register failed\n");
    }
    printk("chardevbase register success\n");
    return 0;
}

/*
描述：驱动出口函数
参数：无
return：无
*/
static void __exit chrdevbase_exit(void)
{
    /*注销字符设备驱动*/
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
    printk("chardevbase_exit()\n");
    printk("chardevbase unregister success\n");
}

/*指定驱动的入口和出口函数*/
module_init(chrdevbase_init);
module_exit(chrdevbase_exit);


/*指定驱动的许可证*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yuanhao");