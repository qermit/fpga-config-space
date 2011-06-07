#include "wb.h"

#define PFX "fakeadc: "

#define WB_CERN_VENDOR 	0x0
#define WB_ADC_DEVICE	0x1

static struct wb_device_id fakeadc_ids[] = {
	{ WB_CERN_VENDOR, WB_ADC_DEVICE},
	{ 0, },
};

static int fakeadc_probe(struct wb_device *dev)
{
	printk(KERN_INFO PFX "found a fake ADC device!\n");
	return 0;
}

static int fakeadc_remove(struct wb_device *dev)
{
	printk(KERN_INFO PFX "removed a fake ADC device!\n");
	return 0;
}

static struct wb_driver fakeadc_driver = {
	.name = "fakeadc",
	.owner = THIS_MODULE,
	.id_table = fakeadc_ids,
	.probe = fakeadc_probe,
	.remove = fakeadc_remove,
};

static int fakeadc_init(void)
{
	return wb_register_driver(&fakeadc_driver);
}

static void fakeadc_exit(void)
{
	wb_unregister_driver(&fakeadc_driver);
}

module_init(fakeadc_init);
module_exit(fakeadc_exit);

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("Fake ADC wishbone driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
