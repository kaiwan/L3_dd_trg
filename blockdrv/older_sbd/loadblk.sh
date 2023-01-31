#!/bin/sh
# Helper script for setting up the block driver on x86
# kaiwan
DRVNAME=sbd
DISKNAME=sblock0
MOUNTPT=/mnt/tmp
NSECT=204800  # 100 MB

[ ! -f ${DRVNAME}.ko ] && {
	echo "$0: block driver ${DRVNAME}.ko not built? aborting..."
	exit 1
}
if [ $(id -u) -ne 0 ]; then
	echo "$0: must run as root."
	exit 1
fi

umount ${MOUNTPT}
mkdir -p ${MOUNTPT} 2>/dev/null
rmmod ${DRVNAME} 2>/dev/null
dmesg -C
make && insmod ./${DRVNAME}.ko nsectors=${NSECT} || exit 1
dmesg
echo "-----cat /proc/partitions"
cat /proc/partitions

# create dev node
MJ=$(grep $DISKNAME /proc/devices |awk '{print $1}')
echo "Major # $MJ"
if [ -z "$MJ" ]; then
	echo "Cannot get major # for dev $DISKNAME, aborting..."
	exit 1
fi
rm -f /dev/${DISKNAME} 2> /dev/null
mknod /dev/${DISKNAME} b ${MJ} 0

echo
echo "-----MUST first create a new partition on the disk $DISKNAME...
'fdisk /dev/$DISKNAME' will now run..
Apply these commands in this order:
n      : New partition
p      : type Primary
1      : part # 1
[Enter]: default first cylinder
[Enter]: default last cylinder
w      : write partition table
"
fdisk /dev/${DISKNAME} || exit 1

echo "-----Creating ext2 filesystem now..."
mkfs /dev/${DISKNAME}p1 || exit 1

umount ${MOUNTPT} 2>/dev/null
mount /dev/${DISKNAME}p1 ${MOUNTPT} || exit 1
echo "-----Mounting new ext2 filesystem now..."
mount |grep $DISKNAME

# done
