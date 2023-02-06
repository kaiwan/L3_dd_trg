#!/bin/bash

# runcmd
# Parameters
#   $1 ... : params are the command to run
runcmd()
{
[ $# -eq 0 ] && return
echo "$@"
eval "$@"
}

DRV=cz_miscdrv
lsmod|grep ${DRV} || {
	echo "First insmod the driver ${DRV} ..."
	exit 1
}

date
runcmd "sudo dmesg -C"
echo "=== Test the czero misc device:"
runcmd "rm -f tst.dat 2>/dev/null"
runcmd "dd if=/dev/czero_miscdev of=tst.dat bs=2k count=3 ; sudo dmesg -c"
runcmd "ls -lh tst.dat"
runcmd "hexdump tst.dat"
echo
echo "--- Test by reading >1 page; should get truncated to 1 page (at a time)..."
runcmd "dd if=/dev/czero_miscdev of=tst.dat bs=6k count=3 ; sudo dmesg -c"

echo
echo "=== Test the cnul misc device:"
echo "--- Redirect to cnul (shouldn't see any o/p of cat):"
runcmd "cat /etc/passwd > /dev/cnul_miscdev"
runcmd "sudo dmesg -c"
runcmd "ls -l /etc/passwd"
echo
echo "--- Test file truncation by writing cnul content to a file"
runcmd "dd if=/dev/urandom of=tst.dat bs=2k count=3"
runcmd "ls -lh tst.dat"
runcmd "cat /dev/cnul_miscdev > tst.dat"
runcmd "ls -lh tst.dat"
runcmd "sudo dmesg -c"

echo
echo "Test both czero and cnul misc devices:"
runcmd "dd if=/dev/czero_miscdev of=/dev/cnul_miscdev bs=8k count=3 ; sudo dmesg -c"
exit 0
