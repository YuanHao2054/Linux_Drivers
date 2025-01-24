#include "stdio.h"
#include "unistd.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "fcntl.h"
#include "stdlib.h"
#include "string.h"

/*测试app要向chrdevbase设备写入的数据*/
static char usrdata[] = {"usr data"};

/*
argc:命令行参数的个数
argv:命令行参数的内容
例如./pragram arg1 arg2 arg3
此时argc=4,
argv[0]="./pragram" 就是程序的名字
argv[1]="arg1"
argv[2]="arg2"
argv[3]="arg3"
*/

/*
./chrdevbaseApp /dev/chrdevbase 1
1表示从设备读取数据
2表示写入数据到设备
*/
int main(int argc, char *argv[])
{
    int fd, retvalue;

    char *filename;
    char readbuf[100], writebuf[100];

    if(argc != 3) //参数不为三，说明输入有误
    {
        printf("Error Usage:%s </dev/chrdevbase> <string>\n", argv[0]);
        return -1;
    }

    filename = argv[1]; //获取设备文件名

    /*打开设备*/
    fd = open(filename, O_RDWR);
    if (fd < 0)
    {
        printf("Can't open file %s\n", filename);
        return -1;
    }

    /*
    atoi()函数是把字符串转换成整型数的一个函数
    atoi("123")的返回值是整型的123
    atoi("abc")的返回值是整型的0
    */

    /*从设备读取数据*/
    if(atoi(argv[2]) == 1)
    {
        retvalue = read(fd, readbuf, 50);
        
        if (retvalue < 0)
        {
            printf("read file %s failed\n", filename);
        }
        else
        {
            printf("read data:%s\n", readbuf);
        }
    }

    if (atoi(argv[2]) == 2)
    {
        /*向设备写入数据*/
        memcpy(writebuf, usrdata, sizeof(usrdata));
        retvalue = write(fd, writebuf, 50);
        if (retvalue < 0)
        {
            printf("write file %s failed\n", filename);
        }
        else
        {
            printf("write data:%s\n", writebuf);
        }
    }

    /*关闭设备*/
    retvalue = close(fd);
    if (retvalue < 0)
    {
        printf("Can't close file %s\n", filename);
        return -1;
    }

    return 0;

}