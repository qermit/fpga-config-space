/*
 * Wishbone bus driver
 *
 * Author: Manohar Vanga <manohar.vanga@cern.ch>
 * Copyright (C) 2011 CERN
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>

#include <linux/wishbone.h>

#include <asm/atomic.h>		/* before 2.6.37 no <linux/atomic.h> */

static void wb_dev_release(struct device *);
static int wb_bus_match(struct device *, struct device_driver *);
static int wb_drv_probe(struct device *);
static int wb_drv_remove(struct device *);

static void wb_zero_release(struct device *dev)
{
}

static struct device wb_dev_zero = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	.bus_id = "wb0",
#else
	.init_name = "wb0",
#endif
	.release = wb_zero_release,
};

static struct bus_type wb_bus_type = {
	.name = "wb",
	.match = wb_bus_match,
	.probe = wb_drv_probe,
	.remove = wb_drv_remove,
};

static void wb_dev_release(struct device *dev)
{
	struct wb_device *wb_dev;

	wb_dev = to_wb_device(dev);

	pr_info(KBUILD_MODNAME ": release %016llx:%08lx\n",
	       wb_dev->wbd.vendor, (unsigned long)wb_dev->wbd.device);

	kfree(wb_dev);
}

static void wb_bus_release(struct device *dev)
{
	struct wb_bus *wb_bus = to_wb_bus(dev);

	pr_info(KBUILD_MODNAME ": release bus %s\n", wb_bus->name);
}

/*
 * Register a Wishbone peripheral on the bus.
 */
int wb_register_device(struct wb_device *wbdev)
{
	static atomic_t global_wb_devno = ATOMIC_INIT(0);
	int devno;

	devno = atomic_inc_return(&global_wb_devno);
	wbdev->dev.bus = &wb_bus_type;
	wbdev->dev.parent = &wbdev->bus->dev;
	wbdev->dev.release = wb_dev_release;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	snprintf(wbdev->dev.bus_id, BUS_ID_SIZE, "wb%d-%s-%s", devno,
		wbdev->wbd.vendor_name, wbdev->wbd.device_name);
#else
	dev_set_name(&wbdev->dev, "wb%d-%s-%s", devno, wbdev->wbd.vendor_name,
		wbdev->wbd.device_name);
#endif
	INIT_LIST_HEAD(&wbdev->list);

	return device_register(&wbdev->dev);
}
EXPORT_SYMBOL(wb_register_device);

/*
 * Unregister a previously registered Wishbone device
 */
void wb_unregister_device(struct wb_device *wbdev)
{
	if (!wbdev)
		return;

	device_unregister(&wbdev->dev);
}
EXPORT_SYMBOL(wb_unregister_device);

/*
 * Register a Wishbone driver
 */
int wb_register_driver(struct wb_driver *driver)
{
	int ret;

	driver->driver.bus = &wb_bus_type;
	driver->driver.name = driver->name;
	ret = driver_register(&driver->driver);

	return ret;
}
EXPORT_SYMBOL(wb_register_driver);

/* Unregister a Wishbone driver */
void wb_unregister_driver(struct wb_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(wb_unregister_driver);

static struct wb_device *wb_get_next_device(struct wb_bus *bus, wb_addr_t wb_ptr)
{
	struct sdwb_wbd wbd;
	struct wb_device *wbdev;

	wb_read_cfg(bus, wb_ptr, (void *)&wbd,
		sizeof(struct sdwb_wbd));

        if (wbd.wbd_magic != SDWB_WBD_MAGIC)
		return NULL;

	wbd.hdl_base = be64_to_cpu(wbd.hdl_base);
	wbd.hdl_size = be64_to_cpu(wbd.hdl_size);

	wbdev = kzalloc(sizeof(struct wb_device), GFP_KERNEL);
	if (!wbdev)
		return NULL;

	memcpy(&wbdev->wbd, &wbd, sizeof(struct sdwb_wbd));
	wbdev->bus = bus;

	return wbdev;
}

void wb_read_sdwb_header(struct wb_bus *bus, struct sdwb_head *head)
{
	wb_read_cfg(bus, bus->sdwb_header_base, (void *)head,
		sizeof(struct sdwb_head));
	head->wbid_address = be64_to_cpu(head->wbid_address);
	head->wbd_address = be64_to_cpu(head->wbd_address);
}

void wb_read_sdwb_id(struct wb_bus *bus, struct sdwb_head *head,
	struct sdwb_wbid *wbid)
{
	wb_read_cfg(bus, head->wbid_address, (void *)wbid,
		sizeof(struct sdwb_wbid));

	wbid->bstream_type = be64_to_cpu(wbid->bstream_type);
	wbid->bstream_version = be64_to_cpu(wbid->bstream_version);
	wbid->bstream_date = be64_to_cpu(wbid->bstream_date);
}

int wb_scan_bus(struct wb_bus *bus)
{
	int ret;
	wb_addr_t wbd_ptr;
	struct sdwb_head head;
	struct sdwb_wbid wbid;
	struct wb_device *wbdev;
	struct wb_device *next;

	wb_read_sdwb_header(bus, &head);

	/* verify our header using the magic field */
	if (head.magic != SDWB_HEAD_MAGIC) {
		pr_err(KBUILD_MODNAME ": invalid sdwb header at %llx "
			"(magic %llx)\n", bus->sdwb_header_base, head.magic);
		return -ENODEV;
	}

	wb_read_sdwb_id(bus, &head, &wbid);
	pr_info(KBUILD_MODNAME ": found sdwb bitstream: 0x%llx\n",
		wbid.bstream_type);

	wbd_ptr = head.wbd_address;

	while ((wbdev = wb_get_next_device(bus, wbd_ptr))) {
		if (wb_register_device(wbdev) < 0) {
			ret = -ENODEV;
			goto err_wbdev_register;
		}

		list_add(&wbdev->list, &bus->devices);
		bus->ndev++;

		wbd_ptr += sizeof(struct sdwb_wbd);
	}

	pr_info(KBUILD_MODNAME
		": found %d wishbone devices on wishbone bus %s\n",
		bus->ndev, bus->name);
	
	return 0;

err_wbdev_register:
	kfree(wbdev);
	list_for_each_entry_safe(wbdev, next, &bus->devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
	}
	bus->ndev = 0;

	return ret;
}

/*
 * Register a Wishbone bus. All devices on the bus will automatically
 * be scanned and registered based on the SDWB specification
 */
int wb_register_bus(struct wb_bus *bus)
{
	static atomic_t global_wbbus_devno = ATOMIC_INIT(0);
	int devno;

	INIT_LIST_HEAD(&bus->devices);
	mutex_init(&bus->dev_lock);

	devno = atomic_inc_return(&global_wbbus_devno);
	bus->num = devno;
	bus->dev.bus = &wb_bus_type;
	bus->dev.parent = &wb_dev_zero;
	bus->dev.release = wb_bus_release;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	snprintf(bus->dev.bus_id, BUS_ID_SIZE, "wb-bus%d-%s", devno,
		bus->name);
#else
	dev_set_name(&bus->dev, "wb-bus%d-%s", devno, bus->name);
#endif

	if (device_register(&bus->dev) < 0) {
		pr_err(KBUILD_MODNAME ": failed to register bus device %d\n",
			devno);
		return -ENODEV;
	}

	if (wb_scan_bus(bus) < 0) {
		pr_err(KBUILD_MODNAME ": failed to scan bus %s\n", bus->name);
		device_unregister(&bus->dev);
		return -EFAULT;
	}

	return 0;
}
EXPORT_SYMBOL(wb_register_bus);

/*
 * Unregister a Wishbone bus. All devices on the bus will be automatically
 * removed as well.
 */
void wb_unregister_bus(struct wb_bus *bus)
{
	struct wb_device *wbdev;
	struct wb_device *next;

	list_for_each_entry_safe(wbdev, next, &bus->devices, list) {
		wb_unregister_device(wbdev);
		list_del(&wbdev->list);
	}
	device_unregister(&bus->dev);
}
EXPORT_SYMBOL(wb_unregister_bus);

/*
 * Match a single Wishbone driver and Wishbone device. An ID of
 * WB_ANY_VENDOR and WB_ANY_DEVICE can be used as a match-all value
 * for these fields.
 */
static struct wb_device_id *wb_match_device(struct wb_driver *drv,
						struct wb_device *dev)
{
	struct wb_device_id *ids;
	uint32_t device = 0;

	ids = drv->id_table;
	if (!ids)
		return NULL;

	while (ids->vendor || (device = ids->device)) {
		if ((ids->dev_class != WB_NO_CLASS &&
			ids->dev_class == dev->wbd.hdl_class) ||
			((ids->vendor == WB_ANY_VENDOR ||
			ids->vendor == dev->wbd.vendor) &&
			(device == WB_ANY_DEVICE ||
			device == dev->wbd.device)))
			return ids;
		ids++;
	}

	return NULL;
}

static int wb_bus_match(struct device *dev, struct device_driver *drv)
{
	struct wb_device *wb_dev;
	struct wb_driver *wb_drv;
	struct wb_device_id *found;

	wb_dev = to_wb_device(dev);
	wb_drv = to_wb_driver(drv);

	found = wb_match_device(wb_drv, wb_dev);
	if (!found)
		return 0;

	/* set this so we can access it later */
	wb_dev->driver = wb_drv;

	return 1;
}

static int wb_drv_probe(struct device *dev)
{
	struct wb_driver *wb_drv;
	struct wb_device *wb_dev;

	wb_dev = to_wb_device(dev);
	wb_drv = wb_dev->driver;
	if (wb_drv && wb_drv->probe)
		return wb_drv->probe(wb_dev);

	return 0;
}

static int wb_drv_remove(struct device *dev)
{
	struct wb_driver *wb_drv;
	struct wb_device *wb_dev;

	wb_dev = to_wb_device(dev);
	wb_drv = wb_dev->driver;
	if (wb_drv && wb_drv->remove)
		return wb_drv->remove(wb_dev);

	return 0;
}

static int wb_init(void)
{
	int ret;

	ret = bus_register(&wb_bus_type);
	if (ret)
		goto bus_reg_fail;

	ret = device_register(&wb_dev_zero);
	if (ret)
		goto device_reg_fail;

	return 0;

device_reg_fail:
	bus_unregister(&wb_bus_type);

bus_reg_fail:
	return ret;
}

static void wb_exit(void)
{
	device_unregister(&wb_dev_zero);
	bus_unregister(&wb_bus_type);
}

module_init(wb_init)
module_exit(wb_exit)

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("Wishbone bus driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
