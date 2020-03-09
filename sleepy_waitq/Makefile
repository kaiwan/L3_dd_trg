ifeq ($(KERNELRELEASE),)  

#########################################
# To support cross-compiling for the ARM:
# For ARM, invoke make as:
# make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabi- 
ifeq ($(ARCH),arm)
# Update 'KDIR' below to point to the ARM Linux kernel source tree
KDIR ?= ~/linux-4.14.0
else
KDIR ?= /lib/modules/$(shell uname -r)/build 
endif
#########################################

PWD := $(shell pwd)  

.PHONY: build clean  

build:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -fv rd_tst wr_tst 2>/dev/null
else
$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
EXTRA_CFLAGS += -DDEBUG
obj-m :=    slpy.o  

endif

rd_tst:
	${CROSS_COMPILE}gcc -Wall -O2 rd_tst.c -o rd_tst
wr_tst:
	${CROSS_COMPILE}gcc -Wall -O2 wr_tst.c -o wr_tst
