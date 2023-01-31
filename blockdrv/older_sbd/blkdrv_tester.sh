#!/bin/sh
# simple test script for sbd driver
DISK=sblock0
MOUNTPT=$(mount |grep ${DISK}|awk '{print $3}')
if [ -z "$MOUNTPT" ]; then
	echo "$0: sbd disk $DISK not mounted?"
	exit 1
fi
if [ ! -d ${MOUNTPT} ]; then
	echo "$0: sbd mount point $MOUNTPT invalid?"
	exit 1
fi
dmesg -C
df -h|grep $DISK

dd if=/dev/urandom of=$MOUNTPT/t1 bs=2048 count=1000
dd if=/dev/urandom of=$MOUNTPT/t2 bs=512 count=3000
dd if=/dev/urandom of=$MOUNTPT/t3 bs=5120 count=100

sync

ls -la $MOUNTPT
df -h|grep $DISK

