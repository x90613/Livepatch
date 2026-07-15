obj-m = loadable_kernel_module.o
PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build
EXTRA_CFLAGS = -Wall -g

all: generateTestFile
	$(MAKE) ARCH=arm64 CROSS_COMPILE=$(CROSS) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

generateTestFile:
	mkdir -p out
	gcc -Wall -Wextra -O2 -I. kill_demo.c -o out/kill_demo

cleanTestFile:
	rm -rf out

load: all
	sudo insmod ./loadable_kernel_module.ko

unload:
	sudo rmmod loadable_kernel_module

logs:
	dmesg | grep -E 'loadable_kernel_module|hacked kill|sys_call_table'

.PHONY: all clean load unload logs generateTestFile cleanTestFile
