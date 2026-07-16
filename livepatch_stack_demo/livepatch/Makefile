obj-m = livepatch_demo.o
PWD  := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS = -Wall -g

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: all
	sudo insmod ./livepatch_demo.ko

unload:
	sudo rmmod livepatch_demo

logs:
	dmesg | grep livepatch_demo

.PHONY: all clean load unload logs
