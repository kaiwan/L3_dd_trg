/*
 * ds3231.c
 * based on my ldm_template_drv.c
 *
 * Reports the temperature via a sysfs file
 * DS3231 datasheet:
 *  https://components101.com/sites/default/files/component_datasheet/DS3231%20Datasheet.pdf
 *
 * (c) 2020 Kaiwan N Billimoria, kaiwanTECH
 * License: Dual MIT/GPL
 */
#define pr_fmt(fmt) "%s:%s():%d: " fmt, KBUILD_MODNAME, __func__, __LINE__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#else
#include <asm/uaccess.h>
#endif
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
//#include <linux/gpio/consumer.h>
#include <linux/bcd.h>
#include "convenient.h"

MODULE_AUTHOR("Kaiwan N Billimoria, kaiwan@kaiwantech.com");
MODULE_DESCRIPTION("DS3231 RTC I2C chip driver");
MODULE_LICENSE("Dual MIT/GPL");

#define GPIO_SQW	16

/* DS3231 RTC registers */
#define A1M1			0x7		// Alarm 1 Seconds
#define A1M2			0x8		// Alarm 1 Minutes
#define A1M3			0x9		// Alarm 1 Hours
#define A1M4			0xA		// Alarm 1 Day/Date

#define TIME_BASEREG		 0x0	// base of RTC time registers
#define TIME_BASEREG_LEN	   7	//  7 bytes..
#define TEMPR_MSB		 	0x11	// Temperature MSB
#define CONTROL_REG			0x0E	// control register

/* Our 'driver data / driver context' data structure */
struct ds3231 {
	struct rtc_device *rtc;
	struct i2c_client *client;
};

static int ds3231_read_block_data(struct i2c_client *client, unsigned char reg,
				  unsigned char length, unsigned char *buf)
{
	struct i2c_msg msgs[] = {
		{		/* setup read ptr */
		 .addr = client->addr,
		 .len = 1,
		 .buf = &reg,
		},
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = buf
		},
	};

	//dev_dbg(&client->dev, "i2c read\nsec,min,hrs,day,date,mon,year\n");
	if ((i2c_transfer(client->adapter, msgs, 2)) != 2) {
		dev_err(&client->dev, "%s: read error\n", __func__);
		return -EIO;
	}

	return 0;
}

static long ds3231_read_temp(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char buf[2];
	int err, len = 2;
	s16 temp;
	long milliC = 0;

	// i2c read yields the temperature value
	err = ds3231_read_block_data(client, TEMPR_MSB, len, buf);
	if (err)
		return err;
	// what about reg 0x12h - Temp LSB ??

	//dev_dbg(dev, "temperature (raw):\n");
	//print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, buf, len);

	/*
	 * Datasheet:
	 * "Temperature is represented as a 10-bit code with a
	 * resolution of 0.25°C and is accessible at location 11h and
	 * 12h. The temperature is encoded in two’s complement
	 * format. The upper 8 bits, the integer portion, are at loca-
	 * tion 11h and the lower 2 bits, the fractional portion, are in
	 * the upper nibble at location 12h. For example,
	 * 0001 1001 01b = +25.25°C. ..."
	 */
	temp = (buf[0] << 8) | buf[1];
	temp >>= 6;
	milliC = temp * 250;
	//pr_info("temp = %d C = %d mC\n", milliC/1000, milliC);
	//pr_info("temp: buf[1] = 0x%x  buf[0] = 0x%x\n", buf[1], buf[0]);

	return milliC;
}

/*------------------ sysfs file (RO) ---------------------------------
 * show Temperature value
 * shows up here:
 *  /sys/bus/i2c/drivers/ds3231/1-0068/ds3231_temp
 * (via several other viewports as well, like
 *  /sys/bus/platform/devices/20804000.i2c/i2c-1/1-0068/ds3231_temp etc)
 *
 * Temperature registers update once every 64s only..
 */
static ssize_t ds3231_temp_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int n;
	long milliC = ds3231_read_temp(dev);

	dev_dbg(dev, "%s(): temp=%ld milliC\n", __func__, milliC);
	n = snprintf(buf, 8, "%ld", milliC);

	return n;
}

/* The DEVICE_ATTR{_RW|RO|WO}() macro instantiates a struct device_attribute
 * dev_attr_<name> here...
 * The name of the 'show' callback function is <foo>_show
 */
static DEVICE_ATTR_RO(ds3231_temp);	/* it's show callback is above.. */


/*
 * Ref: DS3231 datasheet
 * Invoked by /sbin/hwclock (run as: sudo hwclock -v) via the
 * rtc_class_ops->read_time hook
 */
static int ds3231_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char buf[8];
	int err = 0;

	PRINT_CTX();
	//dump_stack();

	/* i2c read yields the rtc timestamp (as first 7 I2C registers, 0x0h to
	 * 0x6h, in BCD
	 */
	err = ds3231_read_block_data(client, TIME_BASEREG, TIME_BASEREG_LEN, buf);
	if (err)
		return err;

	// i2c read yields: sec,min,hrs,day,date,mon,year
	pr_debug("sec,min,hrs,day,date,mon,year\n");
	print_hex_dump_bytes("                           ", DUMP_PREFIX_OFFSET, buf, TIME_BASEREG_LEN);
	tm->tm_sec = bcd2bin(buf[0] & 0xF);
	tm->tm_min = bcd2bin(buf[1] & 0xF);
	tm->tm_hour = bcd2bin(buf[2] & 0xF);
	tm->tm_wday = buf[3] & 0x7;
	tm->tm_mday = bcd2bin(buf[4] & 0xF);
	tm->tm_mon = bcd2bin(buf[5] & 0xF);
	tm->tm_year = bcd2bin(buf[6] & 0xF); // & 0x7F;

	//ds3231_read_temp(dev);

	return 0; //TIME_BASEREG_LEN;
}

static int ds3231_rtc_set_time(struct device *dev, struct rtc_time *t)
{
	struct i2c_client *client = to_i2c_client(dev);
	unsigned char buf[8];
	int err = 0;

	PRINT_CTX();

	  dev_dbg(dev, "%s secs=%d, mins=%d, "
        "hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
        "write:", t->tm_sec, t->tm_min,
        t->tm_hour, t->tm_mday,
        t->tm_mon, t->tm_year, t->tm_wday);


	return 0;
}

/* RELOOK :: ioctl method not called ?? */
static int ds3231_rtc_ioctl(struct device *dev, unsigned int cmd, unsigned long arg)
{
	//struct i2c_client *client = to_i2c_client(dev);
	struct rtc_time tm;

pr_info("############### %s() !\n", __func__);
	PRINT_CTX();
	//QP(dev);
	/* all possible ioctl cmds in include/uapi/linux/rtc.h */
	switch (cmd) {
	case RTC_RD_TIME:
		ds3231_rtc_read_time(dev, &tm);
		if (arg) {
			memset((void *)arg, 0, sizeof(struct rtc_time));
			if (copy_to_user((struct __user rtc_time *)arg, &tm, sizeof(struct rtc_time))) {
				dev_err(dev, "%s: copy_to_user error\n", __func__);
				return -EIO;
			}
		}
		break;
	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static const struct rtc_class_ops ds3231_rtc_ops = {
	.ioctl = ds3231_rtc_ioctl, // nope??
	.read_time = ds3231_rtc_read_time,  // invoked by /sbin/hwclock (run as: sudo hwclock -v)
	.set_time = ds3231_rtc_set_time,
#if 0
	.read_alarm = ds3231_get_alarm,
    .set_alarm  = ds3231_set_alarm,
#endif
};

#if 0
static char *fetch_of_prop(struct device *dev, const char *propname)
{
	char *prop = NULL;
	int len = 0;

	if (dev->of_node)
		prop = of_get_property(dev->of_node, propname, &len);
	return prop;
}
#endif

//static irqreturn_t ds3231_thread_handler(int irq, void *data)
static irqreturn_t ds3231_handler(int irq, void *data)
{
QP;

	return IRQ_HANDLED;
}

static s32 write_i2c_register(struct i2c_client *client, u8 cmdreg, u8 val)
{
	s32 err = 0;

	pr_info("cmdreg=0x%x val=0x%x\n", cmdreg, val);
	err = i2c_smbus_write_byte_data(client, cmdreg, val);
	if (err != 0)
		dev_warn(&client->dev, "register write failed: (0x%x, 0x%x)\n", cmdreg, val);

	return err;
}

/* The probe method of our driver */
static int ds3231_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	/*
	 * first param: struct <foo>_client *client
	 * the specialized structure for this kernel framework (eg. i2c, spi, usb,
	 * pci, etc); we often/usually extract the 'device' pointer from it..
	 */
	struct device *dev = &client->dev;
	struct ds3231 *ds3231;
	int err = 0, irq;
	//struct gpio_desc *gpio;
	//char *res;

	QP;

#if 0
	res = fetch_of_prop(dev, "interrupts");
	if (res)
		pr_info("DT property 'interrupts' = %s\n", res);
	else
		pr_info("DT prop 'interrupts' fetch failed\n");
#endif
	/*
	 * Your work in the probe() routine:
	 * 1. Initialize the device
	 * 2. Prepare driver work: allocate a structure for a suitable
	 *    framework, allocate memory, map I/O memory, register interrupts...
	 * 3. When everything is ready, register the new device to the framework
	 */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	ds3231 = devm_kzalloc(dev, sizeof(struct ds3231), GFP_KERNEL);
	if (!ds3231)
		return -ENOMEM;

	i2c_set_clientdata(client, ds3231);
	ds3231->client = client;

	ds3231->rtc = devm_rtc_allocate_device(&client->dev);
	if (IS_ERR(ds3231->rtc))
		return PTR_ERR(ds3231->rtc);

	ds3231->rtc->ops = &ds3231_rtc_ops;
#if 1  // ??
	ds3231->rtc->uie_unsupported = 0; //1;
	ds3231->rtc->range_min = RTC_TIMESTAMP_BEGIN_2000;
	ds3231->rtc->range_max = RTC_TIMESTAMP_END_2099;
	ds3231->rtc->set_start_time = true;
#endif

	/* Setup the IRQ handler */
	dev_dbg(dev, "client irq = %d\n", client->irq);
#if 0
	dev_dbg(dev, "IRQ: trying via GPIO\n");
    gpio = devm_gpiod_get_optional(dev, "ds3231-sqw",
                           GPIOD_OUT_HIGH);
    if (IS_ERR(gpio))
		dev_dbg(dev, "devm_gpiod_get_optional() failed\n");
    else
		client->irq = gpiod_to_irq(gpio);
#endif

#if 1
/* Via DT; doesn't work.. ?
		 ds3231@68 {
                compatible = "knb,ks3231";
                status = "okay";
                phandle = < 0x80 >;
                reg = < 0x68 >;
                interrupts = < 0x2 170 >;
            };

//#define IRQ_LINE    170
 */

/* TODO :: use gpiolib; gpio_request ?, gpio_direction_output(), ...
 also see: drivers/input/keyboard/matrix_keypad.c:matrix_keypad_parse_dt()
 */

	/* We've attached the DS3231's SQW output to the R Pi's GPIO pin #16 */
	irq = gpio_to_irq(GPIO_SQW);
		// this emits a WARN_ONCE() ! what can be done?
	dev_dbg(dev, "irq = %d\n", irq);

	err = devm_request_threaded_irq(dev, irq, NULL, ds3231_handler,
	//err = devm_request_irq(dev, irq, ds3231_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			//IRQF_SHARED | IRQF_TRIGGER_FALLING,
			//IRQF_SHARED | IRQF_TRIGGER_RISING,
			//IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
			KBUILD_MODNAME, ds3231);
	if (err)
		dev_warn(dev, "threaded IRQ request failed (%d)\n", err);
#endif
	
	// Create our sysfile file : ds3231_temp
	err =
	    device_create_file(dev, &dev_attr_ds3231_temp);
	/* The
	 *  static DEVICE_ATTR_RO(ds3231_temp);
	 * declaration above actually *instantiates* this data structure
	 * &dev_attr_ds3231_temp .
	 * (IOW, the DEVICE_ATTR_XX(name) macro becomes a
	 *  struct device_attribute dev_attr_name data structure!)
	 */
	if (err)
		dev_warn(dev, "device_create_file failed (%d)\n", err);
	dev_dbg(dev, "created sysfs file for reading temperature\n");

	/* Now register ourself with the kernel RTC framework */
	err = rtc_register_device(ds3231->rtc);
	if (err)
		return err;

	/*---
	   test- trigger Alarm1 !
	   set regs A1M1, A1M2, A1M3, A1M5 (0x7, 0x8, 0x9, 0xA) to 0x80 (MSB set);
	   this will cause the alarm to trigger every second

	   s32 i2c_smbus_write_byte_data(const struct i2c_client *client, u8 command,
                  u8 value);
			 command = register to write to
	 */
	write_i2c_register(client, A1M1, 0x80);
	write_i2c_register(client, A1M2, 0x80);
	write_i2c_register(client, A1M3, 0x80);
	write_i2c_register(client, A1M4, 0x80);

	u8 regs[4];
	err = ds3231_read_block_data(client, A1M1, 4, regs);
	if (err) {
		dev_warn(&client->dev, "alarm 0 register read failed: (0x0E, 1)\n");
		return 0;
	}
	print_hex_dump_bytes("alarm0", DUMP_PREFIX_OFFSET, regs, 4);

	/*
	 Control reg 0x0E:
	 bit 2: INTCN : intr ctrl; set to 1;
	  -always set when chip powers on...

	 Bit 1: Alarm 2 Interrupt Enable (A1IE); 0 => disable

	 Bit 0: Alarm 1 Interrupt Enable (A1IE). When set to
	logic 1, this bit permits the alarm 1 flag (A1F) bit in the
	status register to assert INT/SQW (when INTCN = 1)

	So write 0101b = 0x5 ; i.e.  curr_ctrlreg | 0x5
	 */
	u8 ctrlreg_buf[1];
	u8 ctrlreg = 0;

	err = ds3231_read_block_data(client, CONTROL_REG, 1, ctrlreg_buf);
	if (err) {
		dev_warn(&client->dev, "control register read failed: (0x0E, 1)\n");
		return 0;
	}
	ctrlreg = kstrtou8(ctrlreg_buf, 0, &ctrlreg);  //bcd2bin(ctrlreg_buf[0]);
	dev_dbg(&client->dev, "ctrlreg = 0x%08x\n", ctrlreg);
	//print_hex_dump_bytes("ctrl reg bf", DUMP_PREFIX_OFFSET, ctrlreg_buf, 1);

	//if (
	
	ctrlreg |= 0x5;
	write_i2c_register(client, CONTROL_REG, ctrlreg);
	//mb();
	ctrlreg_buf[0] = '\0';
	ds3231_read_block_data(client, CONTROL_REG, 1, ctrlreg_buf);
	ctrlreg = kstrtou8(ctrlreg_buf, 0, &ctrlreg);
	dev_dbg(&client->dev, "ctrlreg af |0x5 = 0x%08x\n", ctrlreg);

	return 0;
}

/* The remove method of our driver */
static int ds3231_remove(struct i2c_client *client)
{
	QP;
	device_remove_file(&client->dev, &dev_attr_ds3231_temp);
	return 0;
}


/*------- Matching the driver to the device ------------------
 * 3 different ways:
 *  by name : for platform & I2C devices
 *  by DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 *  by ACPI ID : for devices on ACPI tables (x86)
 */

/*
 * 1. By name : for platform & I2C devices
 * The <foo>_device_id structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pnp, scsi, sdio, serio, spi, tty, usb, vme
 */
static const struct i2c_device_id ds3231_id[] = {
	{"ks3231", 0},	/* matching by name; required for platform and i2c
					 * devices & drivers
					 */
	{}
};
MODULE_DEVICE_TABLE(i2c, ds3231_id);

/* 2. By DT 'compatible' property : for devices on the Device Tree
 *                                 (ARM32, Aarch64, PPC, etc)
 */
#ifdef CONFIG_OF
static const struct of_device_id ds3231_of_match[] = {
	/* Have deliberately changed the chip's 
	 * compatible poperty  "<manufacturer>,<model>", to our own names as we do NOT
	 * want the default DS compatible driver modprobe'd by default at startup
	 */
	{.compatible = "myvendor,ks3231"},
  //{ .compatible = "maxim,ds3231" },
  //{ .compatible = "<manufacturer>,<model>" },
	{ }
};
MODULE_DEVICE_TABLE(of, ds3231_of_match);
#endif

#if 0
/* 3. By ACPI ID : for devices on ACPI tables (x86) */
static const struct acpi_device_id <chipname>_acpi_id[] = {
     /* x86: matching by ACPI ID */
     {"MMA7660", 0},
     {}
};
MODULE_DEVICE_TABLE(acpi, <chipname>_acpi_id);
static const struct of_device_id dev_ids = {
     {.compatible = "maxim,ds3231"},	//, .data = &pcf_85263_config },
     /* add more here */
}; MODULE_DEVICE_TABLE(of, dev_ids);
#endif

/*
 * The <foo>_driver structure:
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, pnp, scsi, sdio, serio, spi, tty, usb, vme
 */
static struct i2c_driver ds3231_driver = {
	.driver = {
		.name = "ks3231",
		//.name = "ds3231",
			/* platform and I2C use the
			 * 'name' field for the match and thus the bind between the
			 * DT desc/device and driver
			 */
		.of_match_table = of_match_ptr(ds3231_of_match),
				//.of_match_table = of_match_ptr(dev_ids),
	  },
     .probe = ds3231_probe,	// invoked on driver/device bind
     .remove = ds3231_remove,	// optional; invoked on driver/device detach
     .id_table = ds3231_id,
#if 0
     .disconnect = ds3231_disconnect,	// optional; invoked on device disconnect
     .suspend = ds3231_suspend,	// optional
     .resume = ds3231_resume,	// optional
#endif
};

/*
 * Init and Cleanup ::
 * Instead of manually specifiying the init and cleanup handlers in the
 * 'usual' manner, a lot of boilerplate is avoided (when nothing special is
 * required in the init and cleanup code paths) by using the
 *
 * module_foo_driver() macro;
 *
 * where <foo> is typically one of:
 *  acpi_button, cnic, cpufreq, gameport, hid, i2c, ide_pci, ipmi, mbus, mmc,
 *  pci, pnp, scsi, sdio, serio, spi, tty, usb, vme
 * There are several foo_register_driver() APIs; see a list (for 5.4.0) here:
 *  https://gist.github.com/kaiwan/04cfaca711aed9e59282601fafd8aa24
 */
module_i2c_driver(ds3231_driver); /*
		* Register with the i2c bus driver; the module_i2c_driver() macro becomes
		*  i2c_add_driver() which calls i2c_register_driver()
		*/
