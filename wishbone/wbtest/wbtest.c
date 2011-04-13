#include "../wb.h"

int wbtest_match(struct device *dev)
{
	struct wb_device *wbdev = to_wb_device(dev);
	printk("vendor :%d\n", wbdev->vendor);
	return 0;
}

struct wb_device_id wbtest_ids[] = {
	{ WBONE_ANY_ID, WBONE_ANY_ID, WBONE_ANY_ID },
	{ 0, },
};

struct wb_driver wbtest_driver = {
	.version = "wbtest",
	.owner = THIS_MODULE,
	.id_table = wbtest_ids,
};

int wbtest_init(void)
{
	int ret;
	ret = wb_register_driver(&wbtest_driver);
	if (ret < 0)
		return ret;
	return 0;
}

void wbtest_exit(void)
{
	wb_unregister_driver(&wbtest_driver);
}

module_init(wbtest_init);
module_exit(wbtest_exit);
