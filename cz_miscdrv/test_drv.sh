#!/bin/bash
name=$(basename $0)

# runcmd
# Parameters
#   $1 ... : params are the command to run
runcmd()
{
[ $# -eq 0 ] && return
echo "$@"
eval "$@"
}

[ $# -ne 1 ] && {
	echo "Usage : ${name} pathname-to-driver-to-test.ko"
	exit 1
}

DRV=$1
sudo rmmod ${DRV::-3} 2>/dev/null
runcmd "sudo insmod ${DRV}"
lsmod|grep ${DRV::-3} || {
	echo "insmod failed? aborting...
(do you need to build it?)"
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
echo "--- Test by reading >1 page:
with cz_miscdrv     : should get truncated to 1 page (at a time)...
with cz_enh_miscdrv : should transfer ALL requested bytes..."
runcmd "dd if=/dev/czero_miscdev of=tst.dat bs=81k count=3 ; sudo dmesg -c"
runcmd "hexdump tst.dat"

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
