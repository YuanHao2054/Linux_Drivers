#include "fcntl.h"
#include "linux/ioctl.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "unistd.h"
#include "sys/ioctl.h"

/*命令值*/
#define CLOSE_CMD     (_IO(0xEF, 0x1)) /*关闭定时器命令*/
#define OPEN_CMD      (_IO(0xEF, 0x2)) /*打开定时器命令*/
#define SETPERIOD_CMD (_IO(0XEF, 0X3)) /*设置定时器周期命令*/

int main(int argc, char *argv[])
{
    int fd, ret;
    char *filename;
    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];

    if (argc != 2)
    {
        printf("Error Usage!\r\n");
        return -1;
    }

    filename = argv[1];

    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("Can't open file %s\r\n", filename);
        return -1;
    }

    while(1)
    {
        printf("Input cmd: ");
        ret = scanf("%d", &cmd);
        if (ret != 1)
        {
            fgets(str, 100, stdin);
            break;
        }

        if (cmd == 1) /*关闭led*/
        {
            cmd = CLOSE_CMD;
        }
        else if (cmd == 1) /*打开led*/
        {
            cmd = OPEN_CMD;
        }
        else if (cmd == 3)
        {
            cmd = SETPERIOD_CMD; /*设置定时器周期*/
            printf("Input period: ");
            ret = scanf("%d", &arg);
            if (ret != 1)
            {
                fgets(str, 100, stdin);
                break;
            }
        }

        ioctl(fd, cmd, &arg); /*发送命令*/
 

    }
    close(fd);
}