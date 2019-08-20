#!/bin/bash
# load_cz.sh
# Helper script to load the "cz" or "cz-enh" kernel modules,
# also creating the device nodes as necessary.
# 
# Usage:
# load_cz.sh [-option] module ; where option is one of:
# 	-c :	create and load the device files DEV1 and DEV2
# 	-l :	only load the device module into the kernel
# 	--help: show this help screen
#
# * Author: Kaiwan N Billimoria <kaiwan@designergraphix.com>
# * License: [L]GPL


# Configuration variables
DEV1="/dev/czero"
DEV2="/dev/cnul"

PERMS="666"	# for the device files DEV1 and DEV2
MINOR_CZ=1
MINOR_CNUL=2

# Refresh the device nodes
function creat_dev() {
	echo "Creating device files $DEV1 and $DEV2.."

	# Get dynamic major
	# Assumption: the name of the driver has "cz"
	MAJOR=`grep "cz" /proc/devices | awk '{print $1}'`
	if [ -z $MAJOR ]; then
		echo "$0: Could not get major number, aborting.."
		exit 1
	fi
	echo "$0: MAJOR number is $MAJOR"

	# remove any stale instances
	rm $DEV1 2>/dev/null
	rm $DEV2 2>/dev/null

	mknod -m$PERMS $DEV1 c $MAJOR $MINOR_CZ
	if [ $? -ne 0 ]; then
		echo "$DEV1 device creation mknod failure.."
	fi

	/bin/mknod -m$PERMS $DEV2 c $MAJOR $MINOR_CNUL || {
		echo "$DEV2 device creation mknod failure.."
	}
}

function load_dev() { 
	/sbin/rmmod $KMOD 2>/dev/null	# in case it's already loaded

	/sbin/insmod $KMOD || {
		echo "insmod failure.."
		exit 1
	}

	echo "Module $KMOD successfully inserted"
	echo "Looking up last 5 printk's with dmesg.."
	/bin/dmesg | tail -n5
}

function usage() {
	echo "Usage help:: \
 	$0 [-option] module ; 
 where option is one of:
 	-c :	load the driver and create the device files DEV1 and DEV2
 	-l :	only load the device module [module] into the kernel
 	--help: show this help screen
 and module is the filename of the module."
	exit 1
}

# main here

if [ "$(id -u)" != "0" ]; then
	echo "Must be root to load/unload kernel modules"
	exit 1
fi

if [ $# -ne 2 ]; then
	usage
fi

KMOD=$2
if [ ! -e $KMOD ]; then
	echo "$0: $KMOD not a valid file/pathname."
	exit 1 
fi

case "$1" in
   --help) 
	usage
	;;
   -c)
	echo "Loading module and creating device files for device $DEV now.."
	load_dev 
	creat_dev 
	;;	# falls thru to the load..
   -l)
	echo "Loading device $DEV now.."
	load_dev 
	exit 0
	;;
   *)
	usage
	;;
esac

exit 0
