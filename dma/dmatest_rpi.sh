#!/bin/bash
# dmatest_rpi.sh
# Script wrapper over the kernel's dmatest functionality.
# Tested just fine on a Raspberry Pi 3B+ (running a 4.14.114 kernel; the
# dmatest LKM was compiled for it of course).
#
# Ref: https://www.kernel.org/doc/html/latest/driver-api/dmaengine/dmatest.html  
# Kaiwan NB.
name=$(basename $0)
[ $(id -u) -ne 0 ] && {
        echo "${name}: run as root"
        exit 1
}
lsmod|grep dmatest || {
    modprobe dmatest
    lsmod|grep dmatest || {
         echo "${name}: kernel module 'dmatest' not installed? aborting..."
         exit 1
    }
}

# setup
num_iters=3
echo ${num_iters} > /sys/module/dmatest/parameters/iterations
# write empty str to 'channel' to use all available channels
echo "" > /sys/module/dmatest/parameters/channel
echo 1 > /sys/module/dmatest/parameters/verbose

# show params
echo -n "iterations: " ; cat /sys/module/dmatest/parameters/iterations
echo -n "channel: (empty=>all available)" ; cat /sys/module/dmatest/parameters/channel
echo -n "verbose: " ; cat /sys/module/dmatest/parameters/verbose

echo -n "Ok? [y/n] : "
read re
if [ "${re}" != "Y" -a "${re}" != "y" ] ; then
  echo "Exiting now" ; exit 2
fi

# begin!
echo "Starting DMA Test now..."
echo 1 > /sys/module/dmatest/parameters/run

echo 1 > /sys/module/dmatest/parameters/wait

# cleanup
echo "Cleaning up ..."
modprobe -r dmatest
sync
exit 0
