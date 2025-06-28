#!/bin/bash
# Helper script for setting up the (new) block driver on x86_64
# kaiwan n bill, kaiwanTECH

# Turn on Bash 'strict mode'!
# ref: http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail

name=$(basename $0)
DRVNAME=sblkdev
DISKNAME=sblkdev1
MOUNTPT=/mnt/tmp

if [ $(id -u) -ne 0 ]; then
	echo "${name}: must run as root."
	exit 1
fi

umount ${MOUNTPT} 2>/dev/null || true
mkdir -p ${MOUNTPT} 2>/dev/null || true
rmmod ${DRVNAME} 2>/dev/null || true
dmesg -C
[[ ! -f ${DRVNAME}.ko ]] && {
	./mk.sh build || exit 1
}
# use defaults: (params) "sblkdev1,2048;sblkdev2,4096"
insmod ./${DRVNAME}.ko || exit 1
dmesg
# dev nodes auto-created...

echo
echo "${name} :: MUST first create a new partition on the disk $DISKNAME...
'fdisk /dev/$DISKNAME' will now run..
Apply these commands in this order:
n      : New partition
p      : type Primary  [Enter]
1      : partition # 1 [Enter]
1      : First sector (1-2047, default 1): [Enter]
2047   : Last sector, +/-sectors or +/-size{K,M,G,T,P} (1-2047, default 2047): [Enter]
w      : write partition table
"
fdisk /dev/${DISKNAME} || true

echo "${name} : -----Creating ext2 filesystem now (say 'y' if asked) ..."
mkfs /dev/${DISKNAME} || true

echo "${name} : -----Mounting new ext2 filesystem now..."
umount ${MOUNTPT} 2>/dev/null || true
mount /dev/${DISKNAME} ${MOUNTPT} || exit 1
mount |grep $DISKNAME

echo "${name} : 'disk' ready, try it out!"
exit 0
