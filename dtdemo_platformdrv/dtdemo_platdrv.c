
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/of.h>

#define DRVNAME "dtdemo_platdrv"

int dtdemo_platdev_probe(struct platform_device *pdev)
{
	const char *prop = NULL;
	int len = 0;

	pr_info("%s: platform driver probe enter\n", DRVNAME);
	prop = of_get_property(pdev->dev.of_node, "aproperty", &len);
	if (!prop) {
		pr_warn("%s: getting DT property failed\n", DRVNAME);
		return -1;
	}
	pr_info("%s: DT property 'aproperty' = %s (len=%d)\n", DRVNAME, prop, len);
	return 0;
}

int dtdemo_platdev_remove(struct platform_device *pdev)
{
	pr_info("%s: platform driver remove\n", DRVNAME);
	return 0;
}

static const struct of_device_id my_of_ids[] = {
	/* This is esp important: MUST EXACTLY match the 'compatible' property
	   in the DT; even a mismatched space will cause the match to fail! */
	{ .compatible = "dtdemo,dtdemo_platdev"},
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver my_platform_driver = {
	.probe = dtdemo_platdev_probe,
	.remove = dtdemo_platdev_remove,
	.driver = {
		.name = "dtdemo_platdev", /* platform drv driver name must
		  EXACTLY match the DT 'compatible' property for binding to occur;
		  then, the probe method is invoked!  */
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	}
};

static int dtdemo_platdrv_init(void)
{
	int ret_val;
	pr_info("%s: inserted\n", DRVNAME);

	trace_printk("@@@@@ %s: MARKER 1: platform_driver_register() begin\n", DRVNAME);
	ret_val = platform_driver_register(&my_platform_driver);
	trace_printk("@@@@@ %s: MARKER 2: platform_driver_register() done\n", DRVNAME);
	if (ret_val !=0) {
		pr_err("platform value returned %d\n", ret_val);
		return ret_val;

	}
	return 0;
}

static void dtdemo_platdrv_cleanup(void)
{
	platform_driver_unregister(&my_platform_driver);
	pr_info("%s: removed\n", DRVNAME);
}

module_init(dtdemo_platdrv_init);
module_exit(dtdemo_platdrv_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kaiwan N Billimoria");
MODULE_DESCRIPTION("Demo: setting up a DT node, so that the platform drv can get bound");
