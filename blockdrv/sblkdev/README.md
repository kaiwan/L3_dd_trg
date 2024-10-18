# sblkdev
Simple Block Device Linux kernel module
url: https://github.com/CodeImp/sblkdev  \[1\]

ref article: https://prog.world/linux-kernel-5-0-we-write-simple-block-device-under-blk-mq/   \[2\]

Contains a minimum of code to create the most primitive block device.

From \[1\]: 
"The Linux kernel is constantly evolving. And that's fine, but it complicates
the development of out-of-tree modules. I created this out-of-tree kernel
module to make it easier for beginners (and myself) to study the block layer.

Features:
 * Compatible with Linux kernel from 5.10 to 6.0.
 	* 6.8.y - tested on x86_64 Ubuntu 24.04 and Fedora 38 only
 * Allows to create bio-based and request-based block devices.
 * Allows to create multiple block devices.
 * The Linux kernel code style is followed (checked by checkpatch.pl).

How to use (run as root):
* Install kernel headers and compiler
deb:
	`apt install linux-headers gcc make`
	or
	`apt install dkms`
rpm:
	yum install kernel-headers

* Compile module
	`cd ${HOME}/sblkdev; ./mk.sh build`

* Install to current system
	`cd ${HOME}/sblkdev; ./mk.sh install`

* Load module
	`modprobe sblkdev catalog="sblkdev1,2048;sblkdev2,4096"`

* Unload
	`modprobe -r sblkdev`

* Uninstall module
	`cd ${HOME}/sblkdev; ./mk.sh uninstall`

---
**Alternate: Steps to test:**

- `./mk.sh build`
- `sudo ./loadblk.sh`

*< Now partition setup and format of the (pseudo) disk follows >*

'fdisk /dev/sblkdev1' will now run..

Apply these commands in this order:

`n      : New partition`

`p      : type Primary  [Enter]`

`1      : partition # 1 [Enter]`

`1      : First sector (1-2047, default 1): [Enter]`

`2047   : Last sector, +/-sectors or +/-size{K,M,G,T,P} (1-2047, default 2047): [Enter]`

`w      : write partition table`

(*Tip:* Just pressing `[Enter]` typically has the correct defaults setup)

- `./blkdrv_tester.sh`

(This script fires off some disk IO, sleeps for a few seconds, then issues a `sync`. 

*Tip:* keep another terminal window open where you can watch the kernel log as it unfolds; to do so, try :
`journalctl -kf`
).

---
Feedback is welcome.
