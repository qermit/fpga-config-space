#ifndef LINUX_BUS_VBUS_H
#define LINUX_BUS_VBUS_H

#include <linux/device.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <linux/list.h>

#include "sdwb.h"

#define WBONE_ANY_ID (~0)

struct wb_device;

struct wb_device_id {
	__u64 vendor;		/* Vendor or WBONE_ANY_ID */
	__u16 device;		/* Device ID or WBONE_ANY_ID */
};

/*
 * Wishbone Driver Structure
 *
 * @name     : Name of the driver
 * @owner    : The owning module, normally THIS_MODULE
 * @id_table : List of Wishbone ID's this driver supports
 * @probe    : Probe function called on detection of a matching device
 * @remove   : Remove function called on removal of matched device
 * @ops      : Power management ops (not implemented yet)
 * @list     : List for all wishbone drivers
 * @driver   : Internal Linux driver structure
 */
struct wb_driver {
	char *name;
	struct module *owner;
	struct wb_device_id *id_table;
	int (*probe)(struct wb_device *);
	int (*remove)(struct wb_device *);
	const struct dev_pm_ops ops;
	struct list_head list;
	struct device_driver driver;
};
#define to_wb_driver(drv) container_of(drv, struct wb_driver, driver);

/*
 * @name   : Name of the device
 * @wbd    : The wishbone descriptor read from wishbone address space
 * @driver : The driver managing this device
 * @list   : List structure
 * @dev    : Internal Linux device structure
 */
struct wb_device {
	char *name;
	struct sdwb_wbd wbd;
	struct wb_driver *driver;
	struct list_head list;
	struct device dev;
};
#define to_wb_device(dev) container_of(dev, struct wb_device, dev);

int wb_register_device(struct wb_device *wbdev);
void wb_unregister_device(struct wb_device *wbdev);

int wb_register_driver(struct wb_driver *driver);
void wb_unregister_driver(struct wb_driver *driver);

#endif
