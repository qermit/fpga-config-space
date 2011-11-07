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
static int wb_bus_probe(struct device *);
static int wb_bus_remove(struct device *);

static struct device wb_dev_zero = {
	.init_name = "wb0",
	.release = wb_dev_release,
};

static struct bus_type wb_bus_type = {
	.name = "wb",
	.match = wb_bus_match,
	.probe = wb_bus_probe,
	.remove = wb_bus_remove,
};


int wb_read8(struct wb_bus *bus, uint64_t addr, uint8_t *val)
{
	if (bus && bus->ops->read8)
		return bus->ops->read8(addr, val);
	return -EIO;
}

int wb_read16(struct wb_bus *bus, uint64_t addr, uint16_t *val)
{
	if (bus && bus->ops->read16)
		return bus->ops->read16(addr, val);
	return -EIO;
}

int wb_read32(struct wb_bus *bus, uint64_t addr, uint32_t *val)
{
	if (bus && bus->ops->read32)
		return bus->ops->read32(addr, val);
	return -EIO;
}

int wb_read64(struct wb_bus *bus, uint64_t addr, uint64_t *val)
{
	if (bus && bus->ops->read64)
		return bus->ops->read64(addr, val);
	return -EIO;
}

int wb_write8(struct wb_bus *bus, uint64_t addr, uint8_t val)
{
	if (bus && bus->ops->write8)
		return bus->ops->write8(addr, val);
	return -EIO;
}

int wb_write16(struct wb_bus *bus, uint64_t addr, uint16_t val)
{
	if (bus && bus->ops->write16)
		return bus->ops->write16(addr, val);
	return -EIO;
}

int wb_write32(struct wb_bus *bus, uint64_t addr, uint32_t val)
{
	if (bus && bus->ops->write32)
		return bus->ops->write32(addr, val);
	return -EIO;
}

int wb_write64(struct wb_bus *bus, uint64_t addr, uint64_t val)
{
	if (bus && bus->ops->write64)
		return bus->ops->write64(addr, val);
	return -EIO;
}

int memcpy_from_wb(struct wb_bus *bus, uint64_t addr, size_t len,
	uint8_t *buf)
{
	int ret;
	int retlen;

	if (bus && bus->ops->memcpy_from_wb) {
		ret = bus->ops->memcpy_from_wb(addr, len, &retlen, buf);
		if (retlen != len)
			ret = -EIO;
		return ret;
	}
	return -EIO;
}

int memcpy_to_wb(struct wb_bus *bus, uint64_t addr, size_t len,
	const uint8_t *buf)
{
	int ret;
	int retlen;

	if (bus && bus->ops->memcpy_to_wb) {
		ret = bus->ops->memcpy_to_wb(addr, len, &retlen, buf);
		if (retlen != len)
			ret = -EIO;
		return ret;
	}
	return -EIO;
}

static void wb_dev_release(struct device *dev)
{
	struct wb_device *wb_dev;

	wb_dev = to_wb_device(dev);

	pr_info(KBUILD_MODNAME ": release %016llx:%08lx\n",
	       wb_dev->wbd.vendor, (unsigned long)wb_dev->wbd.device);

	kfree(wb_dev);
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
	wbdev->dev.parent = &wb_dev_zero;
	wbdev->dev.release = wb_dev_release;
	dev_set_name(&wbdev->dev, "wb%d", devno);
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
//	INIT_LIST_HEAD(&driver->list);

	return ret;
}
EXPORT_SYMBOL(wb_register_driver);

/* Unregister a Wishbone driver */
void wb_unregister_driver(struct wb_driver *driver)
{
	driver_unregister(&driver->driver);
}
EXPORT_SYMBOL(wb_unregister_driver);

/*
 * Register a Wishbone bus. All devices on the bus will automatically
 * be scanned and registered based on the SDWB specification
 */
int wb_register_bus(struct wb_bus *bus)
{
	int ret;
	int i = 0;
	struct sdwb_head head;
	struct sdwb_wbid wbid;
	struct sdwb_wbd wbd;
	struct wb_device *wbdev;
	struct wb_device *next;

	if (!bus)
		return -ENODEV;

	if (!bus->ops) {
		pr_err(KBUILD_MODNAME ": no wb_ops specified\n");
		return -EFAULT;
	}

	if (!bus->ops->read8 || !bus->ops->read16 || !bus->ops->read32 ||
		!bus->ops->read64 || !bus->ops->write8 || !bus->ops->write16 ||
		!bus->ops->write32 || !bus->ops->write64 ||
		!bus->ops->memcpy_from_wb || !bus->ops->memcpy_to_wb) {
		pr_err(KBUILD_MODNAME ": all ops are required\n");
		return -EFAULT;
	}

	INIT_LIST_HEAD(&bus->devices);

	/*
	 * Scan wb memory and register devices here. Remember to set the
	 * Wishbone devices' 'bus' field to this bus.
	 */

	ret = memcpy_from_wb(bus, bus->sdwb_header_base,
		sizeof(struct sdwb_head), (uint8_t *)&head);
	if (ret < 0) {
		pr_err(KBUILD_MODNAME ": SDWB header read failed\n");
		return ret;
	}

	/* verify our header using the magic field */
	if (head.magic != SDWB_HEAD_MAGIC) {
		pr_err(KBUILD_MODNAME ": invalid sdwb header at %llx "
			"(magic %llx)\n", bus->sdwb_header_base, head.magic);
		return -EFAULT;
	}

	ret = memcpy_from_wb(bus, head.wbid_address, sizeof(struct sdwb_wbid),
		(uint8_t *)&wbid);
	if (ret < 0) {
		pr_err(KBUILD_MODNAME ": SDWB ID read failed\n");
		return ret;
	}

	pr_info(KBUILD_MODNAME ": found sdwb bistream: 0x%llx\n",
		wbid.bstream_type);

	ret = memcpy_from_wb(bus, head.wbd_address, sizeof(struct sdwb_wbd),
		(uint8_t *)&wbd);
	if (ret < 0) {
		pr_err(KBUILD_MODNAME ": SDWB dev read failed\n");
		return ret;
	}

	while (wbd.wbd_magic == SDWB_WBD_MAGIC) {
		wbdev = kzalloc(sizeof(struct wb_device),
			GFP_KERNEL);
		if (!wbdev) {
			ret = -ENOMEM;
			goto err_wbdev_alloc;
		}
		mutex_init(&bus->dev_lock);
		memcpy(&wbdev->wbd, &wbd, sizeof(struct sdwb_wbd));
		wbdev->bus = bus;
		if (wb_register_device(wbdev) < 0) {
			ret = -EFAULT;
			goto err_wbdev_register;
		}
		mutex_lock(&bus->dev_lock);
		list_add(&wbdev->list, &bus->devices);
		bus->ndev++;
		mutex_unlock(&bus->dev_lock);
		ret = memcpy_from_wb(bus,
			head.wbd_address + sizeof(struct sdwb_wbd) * i,
			sizeof(struct sdwb_wbd), (uint8_t *)&wbd);
		if (ret < 0)
			goto err_wbdev_read;
	}

	pr_info(KBUILD_MODNAME
		": found %d wishbone devices on wishbone bus %s\n",
		bus->ndev, bus->name);

	return 0;

err_wbdev_read:
err_wbdev_register:
	kfree(wbdev);
err_wbdev_alloc:
	mutex_lock(&bus->dev_lock);
	list_for_each_entry_safe(wbdev, next, &bus->devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
		kfree(wbdev);
	}
	bus->ndev = 0;
	mutex_unlock(&bus->dev_lock);

	return ret;
}
EXPORT_SYMBOL(wb_register_bus);

/*
 * Unregister a Wishbone bus. All devices on the bus will be automatically
 * removed as well.
 */
void wb_unregister_bus(struct wb_bus *bus)
{
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
	if (found) {
		/* set this so we can access it later */
		wb_dev->driver = wb_drv;
		return 1;
	}
	return 0;
}

static int wb_bus_probe(struct device *dev)
{
	struct wb_driver *wb_drv;
	struct wb_device *wb_dev;

	wb_dev = to_wb_device(dev);
	wb_drv = wb_dev->driver;
	if (wb_drv && wb_drv->probe)
		return wb_drv->probe(wb_dev);

	return 0;
}

static int wb_bus_remove(struct device *dev)
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
