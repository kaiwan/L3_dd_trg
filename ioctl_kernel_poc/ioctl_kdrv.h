/* 
 * ioctl_kdrv.h
 * Header for the ioctl_kdrv.c kernel module (which is a quick demo of using
 * the ioctl() method in a Linux device driver).
 * kaiwan.
 */

/* The 'magic' number for our driver; see Documentation/ioctl-number.txt */
#define IOCTL_KDRV_MAGIC		0x88

/*
 * What is _IO, _IOR|W ? From the LDD3 book (formatted for readability):
--snip--

The header file <asm/ioctl.h>, which is included by <linux/ioctl.h>, defines 
macros that help set up the command numbers as follows: 

_IO(type,nr)                  (for a command that has no argument), 
_IOR(type,nr,datatype)        (for reading data from the driver), 
_IOW(type,nr,datatype)        (for writing data), and 
_IOWR(type,nr,datatype)       (for bidirectional transfers). 

The type and number fields are passed as arguments, and the size field is 
derived by applying sizeof to the datatype argument.

--snip--
*/
/* ioctl (IOC) RESET command */
#define IOCTL_KDRV_IOCRESET		_IO(IOCTL_KDRV_MAGIC, 0)
/* ioctl (IOC) Query POWER command */
#define IOCTL_KDRV_IOCQPOWER		_IOR(IOCTL_KDRV_MAGIC, 1, int)
/* ioctl (IOC) Set POWER command */
#define IOCTL_KDRV_IOCSPOWER		_IOW(IOCTL_KDRV_MAGIC, 2, int)

#define	IOCTL_KDRV_MAXIOCTL		3
