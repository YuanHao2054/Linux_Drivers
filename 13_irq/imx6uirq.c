#include "asm-generic/gpio.h"
#include "asm/atomic.h"
#include "asm/gpio.h"
#include "linux/irqreturn.h"
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
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/types.h>

#define IMX6UIRQ_CNT  1          /*设备号个数*/
#define IMX6UIRQ_NAME "imx6uirq" /*设备名*/
#define KEY0VALUE     0x01       /*KEY0值*/
#define INVAKEY       0xFF       /*无效的KEY值*/
#define KEY_NUM       1          /*按键数量*/

/*中断IO描述结构体*/
struct irq_keydesc
{
    int gpio;                            /*GPIO*/
    int key_val;                         /*键值*/
    unsigned char value;                 /*按键对应的键值*/
    char name[10];                       /*名字*/
    irqreturn_t (*handler)(int, void *); /*中断处理函数*/
};

/*imx6uirq设备结构体*/
struct imx6uirq_dev
{
    dev_t devid;                            /*设备号*/
    struct cdev cdev;                       /*cdev*/
    struct class *class;                    /*类*/
    struct device *device;                  /*设备*/
    int major;                              /*主设备号*/
    int minor;                              /*次设备号*/
    struct device_node *nd;                 /*设备节点*/
    atomic_t keyvalue;                      /*按键值*/
    atomic_t releasekey;                    /*按键松开标志*/
    struct timer_list timer;                /*定时器*/
    struct irq_keydesc irqkeydesc[KEY_NUM]; /*按键描述数组*/
    unsigned char curkeynum;                /*当前按键号*/
};

struct imx6uirq_dev imx6uirq; /*imx6uirq设备*/

/*中断服务函数*/
static irqreturn_t key0_handler(int irq, void *dev_id)
{

    /*触发中断后，获取设备id*/
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)dev_id;
    dev->curkeynum = 0;
    dev->timer.data = (volatile long)dev_id;

    /*启动定时器*/
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));
    return IRQ_RETVAL(IRQ_HANDLED);

}

/*定时器中断服务函数*/
/*用于按键消抖，定时器到了以后再读取按键值，如果还是处于按下状态，就表示按键有效*/
void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct imx6uirq_dev *dev = (struct imx6uirq_dev *)arg;

    num = dev->curkeynum;
    keydesc = &dev->irqkeydesc[num];

    /*读取按键值*/
    value = gpio_get_value_cansleep(keydesc->gpio);

    if (value == 0) /*如果此时按钮还是按下*/
    {
        atomic_set(&dev->keyvalue, keydesc->key_val);
    }
    else /*如果此时按键已经松开*/
    {
        atomic_set(&dev->keyvalue, 0x80 | keydesc->value);
    }
}