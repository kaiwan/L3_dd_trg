ifeq ($(KERNELRELEASE),)  

#########################################
# To support cross-compiling for the ARM:
# For ARM, invoke make as:
# make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- 
ifeq ($(ARCH),arm)
# Update 'KDIR' below to point to the ARM Linux kernel source tree
KDIR ?= ~/3.10.24
else
KDIR ?= /lib/modules/$(shell uname -r)/build 
endif
#########################################

PWD := $(shell pwd)  

.PHONY: build clean  

build:
	$(MAKE) -Wall -C $(KDIR) M=$(PWD) modules
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) clean

else

$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
EXTRA_CFLAGS += -DDEBUG -g -ggdb
obj-m :=    sbd.o  

endif

