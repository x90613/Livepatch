obj-m = function_pointer_kill.o
PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS = -Wall -g

all:
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: all
	sudo insmod ./function_pointer_kill.ko

unload:
	sudo rmmod function_pointer_kill

logs:
	dmesg | grep function_pointer_kill

.PHONY: all clean load unload logs
