/*
 * Linux input driver for a wireless USB laser presenter device.
 * (The model I happen to currently have is one from Promotion & Display Technology:
 *   0d8d:1516 Promotion & Display Technology, Ltd Wireless Present
 * Kaiwan NB.
 *
 * Credits:
 * Based on the GPL-ed 'USB-driver' by @AcollaMolla (Anton Engman)
 *  https://github.com/AcollaMolla/USB-driver
 */
#define pr_fmt(fmt) "%s:%s(): " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/usb/input.h>
#include <linux/hid.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ANTON, Kaiwan");
MODULE_DESCRIPTION("Simple input driver on USB bus for a wireless laser presenter");
MODULE_VERSION("0.1");

static int dev_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void dev_disconnect(struct usb_interface *interface);
static int usb_laserpdl_open(struct input_dev *dev);
static void usb_laserpdl_close(struct input_dev *dev);

struct usb_laserpdl {
	char name[128];
	char phys[64];
	struct usb_device *usbdev;
	struct input_dev *dev;
	struct urb *irq_urb;
	signed char *data;
	dma_addr_t data_dma;
};

static void usb_laserpdl_irq(struct urb *urb)
{
	struct usb_laserpdl *laserpdl = urb->context;
	signed char *data = laserpdl->data;
	struct input_dev *dev = laserpdl->dev;
	int status;

	pr_debug("urb stat=%d\n", urb->status);

/*
TODO / RELOOK:
ERROR:
...
[  865.106942] mymouse_usb:usb_wimouse_irq(): urb stat=-2

reset by

-check which is the latest /dev/input/eventN file
ls -lt /dev/input
and
read it via evtest

??

Seems to Require that inputX is READ by something/anything, then it works..
(this issue has come up on SO, etc)
*/

	switch (urb->status) {
	case 0:		/* success */
		break;
	case -ECONNRESET:	/* unlink */
	case -ENOENT:		// -2
	case -ESHUTDOWN:	// -108
		/* This urb is terminated, clean up */
		dev_dbg(&dev->dev, "%s - urb shutting down with status: %d\n",
			__func__, urb->status);
		return;
		/* -EPIPE:  should clear the halt */
	default:		/* error */
		goto resubmit;
	}

 resubmit:
	pr_debug("resubmit urb\n");
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		dev_err(&laserpdl->usbdev->dev,
			"can't resubmit intr, %s-%s/input0, status %d\n",
			laserpdl->usbdev->bus->bus_name, laserpdl->usbdev->devpath, status);

	int i;
	for (i = 0; i < 8; i++) {
		if (data[i])
			pr_debug("data[%d]=0x%x (%d)\n", i, data[i], data[i]);
	}

	// Report which buttons / rel x,y was pressed
	input_report_key(dev, KEY_PAGEUP, (data[3] == 0x4b));
	input_report_key(dev, KEY_PAGEDOWN, (data[3] == 0x4e));
	input_report_key(dev, KEY_UP, ((data[3] == 0x3e) || (data[3] == 0x29)));
	input_report_key(dev, KEY_DOWN, (data[3] == 0x5));

	input_sync(dev);
}

static int dev_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct device *dev = &usbdev->dev;
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_laserpdl *laserpdl;
	struct input_dev *input_dev;
	int pipe, maxp;
	int error = -ENOMEM;

	/* A lot of the following probe code appears to be pretty identical to
	 * drivers/usb/storage/onetouch.c
	 */
	interface = intf->cur_altsetting;

	pr_info("USB interface %d now probed: (%04X:%04X)\n",
		interface->desc.bInterfaceNumber, id->idVendor, id->idProduct);
	pr_info("Number of endpoints: %02X\n", interface->desc.bNumEndpoints);
	pr_info("Interface class: %02X\n", interface->desc.bInterfaceClass);

	if (interface->desc.bNumEndpoints != 1)
		return -ENODEV;

	endpoint = &interface->endpoint[0].desc;
	/* Must be an Interrupt
	 * type endpoint w/ dir as IN (i.e. it sends data IN to the host computer)
	 */
	if (!usb_endpoint_is_int_in(endpoint))
		return -ENODEV;

	pipe = usb_rcvintpipe(usbdev, endpoint->bEndpointAddress);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 19, 0)
	maxp = usb_maxpacket(usbdev, pipe);
#else
	maxp = usb_maxpacket(usbdev, pipe, usb_pipeout(pipe));
#endif

	laserpdl = kzalloc(sizeof(struct usb_laserpdl), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!input_dev)
		goto fail1;

	laserpdl->data = usb_alloc_coherent(usbdev, 8, GFP_ATOMIC, &laserpdl->data_dma);
	if (!laserpdl->data)
		goto fail1;

	laserpdl->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!laserpdl->irq_urb)
		goto fail2;

	laserpdl->usbdev = usbdev;
	laserpdl->dev = input_dev;

	if (usbdev->manufacturer)
		strscpy(laserpdl->name, usbdev->manufacturer, sizeof(laserpdl->name));

	if (usbdev->product) {
		if (usbdev->manufacturer)
			strlcat(laserpdl->name, " ", sizeof(laserpdl->name));
		strlcat(laserpdl->name, usbdev->product, sizeof(laserpdl->name));
	}

	if (!strlen(laserpdl->name))
		snprintf(laserpdl->name, sizeof(laserpdl->name),
			 "USB <unknown device> %04x:%04x",
			 le16_to_cpu(usbdev->descriptor.idVendor),
			 le16_to_cpu(usbdev->descriptor.idProduct));

	usb_make_path(usbdev, laserpdl->phys, sizeof(laserpdl->phys));
	/* Why append '/input0'?
	 * Because "... the event's sent out the door via /dev/input/eventX,
	 *  where X is the *interface number* assigned to the vms driver." (ELDD)
	 */
	strlcat(laserpdl->phys, "/input0", sizeof(laserpdl->phys));
	dev_dbg(dev, "usb path=%s\n", laserpdl->phys);

	input_dev->name = laserpdl->name;
	input_dev->phys = laserpdl->phys;
	usb_to_input_id(usbdev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	/*
	 * The USB wireless laser presenter has 4 buttons:
	 * left (use as PageUp), right (use as PageDn) and 2 others...
	 */
	// Add button events
	// see include/uapi/linux/input-event-codes.h
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_PAGEUP, input_dev->keybit);
	set_bit(KEY_PAGEDOWN, input_dev->keybit);
	set_bit(KEY_UP, input_dev->keybit);
	set_bit(KEY_DOWN, input_dev->keybit);

	input_set_drvdata(input_dev, laserpdl);

	input_dev->open = usb_laserpdl_open;
	input_dev->close = usb_laserpdl_close;

	usb_fill_int_urb(laserpdl->irq_urb, usbdev, pipe, laserpdl->data,
			 (maxp > 8 ? 8 : maxp), usb_laserpdl_irq, laserpdl, endpoint->bInterval);
	laserpdl->irq_urb->transfer_dma = laserpdl->data_dma;
	laserpdl->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/*
	 * The driver 'probe' method typically registers with a kernel framework;
	 * here, we register with the kernel 'input' framework
	 */
	error = input_register_device(laserpdl->dev);
	if (error)
		goto fail3;

	usb_set_intfdata(intf, laserpdl);

	return 0;

 fail3:
	usb_free_urb(laserpdl->irq_urb);
 fail2:
	usb_free_coherent(usbdev, 8, laserpdl->data, laserpdl->data_dma);
 fail1:
	input_free_device(input_dev);
	kfree(laserpdl);
	return error;
}

static void dev_disconnect(struct usb_interface *intf)
{
	//struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_laserpdl *laserpdl = usb_get_intfdata(intf);
	struct input_dev *input_dev;

	if (laserpdl) {
		pr_info("USB disconnecting\n");
		input_dev = laserpdl->dev;
		input_set_drvdata(input_dev, NULL);
		//      input_unregister_device(input_dev);
		usb_deregister_dev(intf, NULL);
	}
}

static int usb_laserpdl_open(struct input_dev *dev)
{
	struct usb_laserpdl *laserpdl;

	pr_debug("usb laserpdl open\n");
	laserpdl = input_get_drvdata(dev);
	if (!laserpdl) {
		pr_info("no laserpdl\n");
		return -1;
	}
	if (!laserpdl->usbdev) {
		pr_info("no laserpdl->usbdev\n");
		return -1;
	}
	if (!laserpdl->irq_urb) {
		pr_info("No laserpdl->irq_urb\n");
		return -1;
	}
	pr_debug("irq ok\n");
	laserpdl->irq_urb->dev = laserpdl->usbdev;

	if (usb_submit_urb(laserpdl->irq_urb, GFP_KERNEL)) {
		pr_info("Failed submiting urb\n");
		return -1;
	}
	pr_debug("submit urb ok\n");

	return 0;
}

static void usb_laserpdl_close(struct input_dev *dev)
{
	struct usb_laserpdl *laserpdl = input_get_drvdata(dev);

	usb_kill_urb(laserpdl->irq_urb);
}

// laser pointer with 4 buttons
// 0d8d:1516 Promotion & Display Technology, Ltd Wireless Present   (PDL)
#define USB_PROMODISPLAYTECH_VID   0x0d8d
#define USB_PROMODISPLAYTECH_PID   0x1516

static struct usb_device_id dev_table[] = {
	{USB_DEVICE(USB_PROMODISPLAYTECH_VID, USB_PROMODISPLAYTECH_PID)},
	{}
};

MODULE_DEVICE_TABLE(usb, dev_table);
static struct usb_driver laserpdl_driver = {
	.name = "laserp",
	.id_table = dev_table,
	.probe = dev_probe,
	.disconnect = dev_disconnect
};

module_usb_driver(laserpdl_driver);
