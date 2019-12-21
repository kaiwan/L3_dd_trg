# Linux 2.6 kernel module Makefile
# orig src from- 
# http://www.linux.com/news/software/linux-kernel/23685-the-kernel-newbie-corner-your-first-loadable-kernel-module

ifeq ($(KERNELRELEASE),)  

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)  

.PHONY: build clean  

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
clean:
	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) clean
#	rm -f mmap_tst

mmap_tst: mmap_tst.c
	gcc mmap_tst.c -o mmap_tst -Wall

else

$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
EXTRA_CFLAGS += -DDEBUG
obj-m :=    cz_mmap.o

endif

