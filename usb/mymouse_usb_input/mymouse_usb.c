/*
 * Linux input driver for a wireless USB mouse.
 * (The model I happen to currently have is one from Dell:
 *   04ca:00a8 Lite-On Technology Corp. Dell Wireless Mouse WM118
 * To see more on how wireless (USB receiver) mice work, see the doc/readme).
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
MODULE_DESCRIPTION("Simple input driver on USB bus for a wireless mouse");
MODULE_VERSION("0.1");

static int dev_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void dev_disconnect(struct usb_interface *interface);
static int usb_wimouse_open(struct input_dev *dev);
static void usb_wimouse_close(struct input_dev *dev);

struct usb_wimouse {
	char name[128];
	char phys[64];
	struct usb_device *usbdev;
	struct input_dev *dev;
	struct urb *irq_urb;
	signed char *data;
	dma_addr_t data_dma;
};


/*
 * This is the 'response' portion of the USB URB request-response mechanism
 * - the (in effect) IRQ handler. It runs in interrupt (atomic) context - don't
 * sleep!
 *
 * Ref: https://www.kernel.org/doc/html/latest/driver-api/usb/URB.html#what-about-the-completion-handler 
 */
static void usb_wimouse_irq(struct urb *urb)
{
	struct usb_wimouse *wimouse = urb->context;
	signed char *data = wimouse->data;
	struct input_dev *dev = wimouse->dev;
	int status;

	//pr_debug("urb stat=%d\n", urb->status);

/*
TODO / RELOOK:
Sometimes get this ERROR:
...
[  865.106942] mymouse_usb:usb_wimouse_irq(): urb stat=-2

Eliminate it by:
-checking which is the latest /dev/input/eventN file
ls -lt /dev/input
- *read* it via evtest / whatever

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

	/*
	 * Why resubmit the URB?
	 * From official kernel doc (link is shown above):
	 * "In Linux 2.6, unlike earlier versions, interrupt URBs are not
	 * automagically restarted when they complete. They end when the
	 * completion handler is called, just like other URBs. If you want an
	 * interrupt URB to be restarted, your completion handler must resubmit it"
	 * Also, as it's asynchoronous, the submit succeeds and the completion
	 * handler continues to run to completion..
	 */
 resubmit:
	//pr_debug("resubmit urb\n");
	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status)
		dev_err(&wimouse->usbdev->dev,
			"can't resubmit intr, %s-%s/input0, status %d\n",
			wimouse->usbdev->bus->bus_name, wimouse->usbdev->devpath, status);

#ifdef DEBUG
          int i;
          for (i = 0; i < 8; i++) {
                  if (data[i])
                          pr_debug("data[%d]=0x%x=%d\n", i, data[i], data[i]);
          }
#endif
	// Report which buttons or relative (x,y) was pressed/moved
	input_report_key(dev, BTN_LEFT, data[1] & 0x1);
	input_report_key(dev, BTN_RIGHT, data[1] & 0x2);
	input_report_key(dev, BTN_MIDDLE, data[1] & 0x4);
	input_report_key(dev, KEY_UP, (data[5] == 0x1 ? 1 : 0));
	input_report_key(dev, KEY_DOWN, (data[5] == -1 ? 1 : 0));

	// report the delta (x,y)
	input_report_rel(dev, REL_X, data[2]);
	input_report_rel(dev, REL_Y, data[3]);

	input_sync(dev);
}

static int dev_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	struct usb_device *usbdev = interface_to_usbdev(intf);
	struct device *dev = &usbdev->dev;
	struct usb_host_interface *interface;
	struct usb_endpoint_descriptor *endpoint;
	struct usb_wimouse *wimouse;
	struct input_dev *input_dev;
	int pipe, maxp;
	int error = -ENOMEM;

	/* A lot of the following probe code appears to be pretty identical to
	 * drivers/hid/usbhid/usbmouse.c
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

	wimouse = kzalloc(sizeof(struct usb_wimouse), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!input_dev)
		goto fail1;

	wimouse->data = usb_alloc_coherent(usbdev, 8, GFP_ATOMIC, &wimouse->data_dma);
	if (!wimouse->data)
		goto fail1;

	wimouse->irq_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!wimouse->irq_urb)
		goto fail2;

	wimouse->usbdev = usbdev;
	wimouse->dev = input_dev;

	if (usbdev->manufacturer)
		strscpy(wimouse->name, usbdev->manufacturer, sizeof(wimouse->name));

	if (usbdev->product) {
		if (usbdev->manufacturer)
			strlcat(wimouse->name, " ", sizeof(wimouse->name));
		strlcat(wimouse->name, usbdev->product, sizeof(wimouse->name));
	}

	if (!strlen(wimouse->name))
		snprintf(wimouse->name, sizeof(wimouse->name),
			 "USB <unknown device> %04x:%04x",
			 le16_to_cpu(usbdev->descriptor.idVendor),
			 le16_to_cpu(usbdev->descriptor.idProduct));

	usb_make_path(usbdev, wimouse->phys, sizeof(wimouse->phys));
	/* Why append '/input0'?
	 * Because "... the event's sent out the door via /dev/input/eventX,
	 *  where X is the *interface number* assigned to the vms driver." (ELDD)
	 */
	strlcat(wimouse->phys, "/input0", sizeof(wimouse->phys));
	dev_dbg(dev, "usb path=%s\n", wimouse->phys);

	input_dev->name = wimouse->name;
	input_dev->phys = wimouse->phys;
	usb_to_input_id(usbdev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;

	/*
	 * The USB wireless mouse has x,y relative events and 3 buttons:
	 * BTN_LEFT, BTN_MIDDLE and BTN_RIGHT
	 * In addition, the mouse wheel can produce 3 events:
	 *  middle press (handled by BTN_MIDDLE), scroll up, scroll down
	 */
	// Add relative ('rel') x,y events; they're actually the delta from the prev (x,y)
	set_bit(EV_REL, input_dev->evbit);
	set_bit(REL_X, input_dev->relbit);	// rel 'x' movement
	set_bit(REL_Y, input_dev->relbit);	// rel 'y' movement

	// Add button and/or relative (rel) events
	// see include/uapi/linux/input-event-codes.h
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_LEFT, input_dev->keybit);
	set_bit(BTN_MIDDLE, input_dev->keybit);
	set_bit(BTN_RIGHT, input_dev->keybit);
	set_bit(KEY_UP, input_dev->keybit);
	set_bit(KEY_DOWN, input_dev->keybit);

	input_set_drvdata(input_dev, wimouse);

	input_dev->open = usb_wimouse_open;
	input_dev->close = usb_wimouse_close;

	/* USB request-response:
	 * Here we setup the request by filling in the URB (receive-interrupt type).
	 * We submit it (via usb_submit_urb()) in the input device open() method..
	 * It's async; the completion handler - in effect the irq handler - runs
	 * and in it we check if all's ok, processing the data, sending it up to
	 * the input layer...
	 * Ref: https://www.kernel.org/doc/html/latest/driver-api/usb/URB.html
	 */
	usb_fill_int_urb(wimouse->irq_urb, usbdev, pipe, wimouse->data,
			 (maxp > 8 ? 8 : maxp), usb_wimouse_irq, wimouse, endpoint->bInterval);
	wimouse->irq_urb->transfer_dma = wimouse->data_dma;
	wimouse->irq_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/*
	 * The driver 'probe' method typically registers with a kernel framework;
	 * here, we register with the kernel 'input' framework
	 */
	error = input_register_device(wimouse->dev);
	if (error)
		goto fail3;

	usb_set_intfdata(intf, wimouse);

	return 0;

 fail3:
	usb_free_urb(wimouse->irq_urb);
 fail2:
	usb_free_coherent(usbdev, 8, wimouse->data, wimouse->data_dma);
 fail1:
	input_free_device(input_dev);
	kfree(wimouse);
	return error;
}

static void dev_disconnect(struct usb_interface *intf)
{
	//struct usb_device *usbdev = interface_to_usbdev(intf);
	struct usb_wimouse *wimouse = usb_get_intfdata(intf);
	struct input_dev *input_dev;

	if (wimouse) {
		pr_info("USB disconnecting\n");
		input_dev = wimouse->dev;
		input_set_drvdata(input_dev, NULL);
		//      input_unregister_device(input_dev);
		usb_deregister_dev(intf, NULL);
	}
}

static int usb_wimouse_open(struct input_dev *dev)
{
	struct usb_wimouse *wimouse;

	pr_debug("usb wimouse open\n");
	wimouse = input_get_drvdata(dev);
	if (!wimouse) {
		pr_info("no wimouse\n");
		return -1;
	}
	if (!wimouse->usbdev) {
		pr_info("no wimouse->usbdev\n");
		return -1;
	}
	if (!wimouse->irq_urb) {
		pr_info("No wimouse->irq_urb\n");
		return -1;
	}
	pr_debug("irq ok\n");
	wimouse->irq_urb->dev = wimouse->usbdev;

	// URB submit 'request'; 'response' is the completion (irq) handler
	if (usb_submit_urb(wimouse->irq_urb, GFP_KERNEL)) {
		pr_info("Failed submiting urb\n");
		return -1;
	}
	pr_debug("submit urb ok\n");

	return 0;
}

static void usb_wimouse_close(struct input_dev *dev)
{
	struct usb_wimouse *wimouse = input_get_drvdata(dev);

	usb_kill_urb(wimouse->irq_urb);
}

// 04ca:00a8 Lite-On Technology Corp. Dell Wireless Mouse WM118
#define USB_DELL_WIRELESS_MOUSE_VID   0x04ca
#define USB_DELL_WIRELESS_MOUSE_PID   0x00a8

static struct usb_device_id dev_table[] = {
	{USB_DEVICE(USB_DELL_WIRELESS_MOUSE_VID, USB_DELL_WIRELESS_MOUSE_PID)},
//	{USB_DEVICE(0x046d,0xc52e)}, // Logitech MK260 Wireless Combo Receiver
	{}
};

MODULE_DEVICE_TABLE(usb, dev_table);
static struct usb_driver wimousep_driver = {
	.name = "mymouse_usb",
	.id_table = dev_table,
	.probe = dev_probe,
	.disconnect = dev_disconnect
};

module_usb_driver(wimousep_driver);
