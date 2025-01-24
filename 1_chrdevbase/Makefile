export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-


KERNELDIR := /home/yuanhao/linux/alientek_linux
CURRENT_PATH := $(shell pwd)
obj-m := chrdevbase.o

build: kernel_modules
kernel_modules:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) modules
clean:
	$(MAKE) -C $(KERNELDIR) M=$(CURRENT_PATH) clean