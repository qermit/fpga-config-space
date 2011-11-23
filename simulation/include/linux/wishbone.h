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

#ifndef _LINUX_WISHBONE_H
#define _LINUX_WISHBONE_H

#include <linux/device.h>
#include <linux/pm.h>
#include <linux/types.h>
#include <linux/list.h>

#include "sdwb.h"

#define WB_ANY_VENDOR ((uint64_t)(~0))
#define WB_ANY_DEVICE ((uint32_t)(~0))
#define WB_NO_CLASS ((uint32_t)(~0))

/* Wishbone I/O operations */
#define wb_readb(bus, addr) bus->ops->read8(bus, addr)
#define wb_readw(bus, addr) bus->ops->read16(bus, addr)
#define wb_readl(bus, addr) bus->ops->read32(bus, addr)
#define wb_readll(bus, addr) bus->ops->read64(bus, addr)

#define wb_writeb(bus, addr, val) bus->ops->write8(bus, addr, val)
#define wb_writew(bus, addr, val) bus->ops->write16(bus, addr, val)
#define wb_writel(bus, addr, val) bus->ops->write32(bus, addr, val)
#define wb_writell(bus, addr, val) bus->ops->write64(bus, addr, val)

#define memcpy_from_wb(bus, addr, buf, len) \
	bus->ops->memcpy_from_wb(bus, addr, buf, len)

#define memcpy_to_wb(bus, addr, buf, len) \
	bus->ops->memcpy_to_wb(bus, addr, buf, len)

#define wb_read_cfg(bus, addr, buf, len) \
	bus->ops->read_cfg(bus, addr, buf, len)

typedef uint64_t wb_addr_t;

struct wb_device;

struct wb_device_id {
	uint64_t vendor;	/* Vendor ID or WB_ANY_VENDOR */
	uint32_t device;	/* Device ID or WB_ANY_DEVICE */
	uint32_t dev_class;	/* Class ID or WB_NO_CLASS */
};

/*
 * Wishbone Driver Structure
 *
 * @name     : Name of the driver
 * @owner    : The owning module, normally THIS_MODULE
 * @id_table : Zero-terminated table of Wishbone ID's this driver supports
 * @probe    : Probe function called on detection of a matching device
 * @remove   : Remove function called on removal of matched device
 * @driver   : Internal Linux driver structure
 */
struct wb_driver {
	char *name;
	struct module *owner;
	struct wb_device_id *id_table;
	int (*probe)(struct wb_device *);
	int (*remove)(struct wb_device *);
	struct device_driver driver;
};
#define to_wb_driver(drv) container_of(drv, struct wb_driver, driver);

/*
 * @wbd    : The wishbone descriptor read from wishbone address space
 * @driver : The driver managing this device
 * @list   : List of Wishbone devices (per bus)
 * @dev    : Internal Linux device structure
 * @bus    : The bus the device is on
 * @priv   : Private data pointer for use by drivers
 */
struct wb_device {
	struct sdwb_wbd wbd;
	struct wb_driver *driver;
	struct list_head list;
	struct device dev;
	struct wb_bus *bus;
	void *priv;
};
#define to_wb_device(dev) container_of(dev, struct wb_device, dev);

struct wb_ops {
	uint8_t (*read8)(struct wb_bus *, wb_addr_t);
	uint16_t (*read16)(struct wb_bus *, wb_addr_t);
	uint32_t (*read32)(struct wb_bus *, wb_addr_t);
	uint64_t (*read64)(struct wb_bus *, wb_addr_t);
	void (*write8)(struct wb_bus *, wb_addr_t, uint8_t);
	void (*write16)(struct wb_bus *, wb_addr_t, uint16_t);
	void (*write32)(struct wb_bus *, wb_addr_t, uint32_t);
	void (*write64)(struct wb_bus *, wb_addr_t, uint64_t);
	void * (*memcpy_from_wb) (struct wb_bus *, wb_addr_t addr, void *buf,
		size_t len);
	void * (*memcpy_to_wb) (struct wb_bus *, wb_addr_t addr,
		const void *buf, size_t len);
	void * (*read_cfg)(struct wb_bus *, wb_addr_t addr, void *buf,
		size_t len);
};

struct wb_bus {
	int num;
	char *name;
	struct module *owner;
	struct device dev;
	wb_addr_t sdwb_header_base;
	struct wb_ops *ops;
	struct list_head devices;
	struct mutex dev_lock;
	int ndev;
};
#define to_wb_bus(dev) container_of(dev, struct wb_bus, dev);

int wb_register_driver(struct wb_driver *driver);
void wb_unregister_driver(struct wb_driver *driver);

int wb_register_bus(struct wb_bus *bus);
void wb_unregister_bus(struct wb_bus *bus);

#endif /* _LINUX_WISHBONE_H */

