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
 * @list     : List for all wishbone drivers
 * @driver   : Internal Linux driver structure
 */
struct wb_driver {
	char *name;
	struct module *owner;
	struct wb_device_id *id_table;
	int (*probe)(struct wb_device *);
	int (*remove)(struct wb_device *);
//	struct list_head list;
	struct device_driver driver;
};
#define to_wb_driver(drv) container_of(drv, struct wb_driver, driver);

/*
 * @wbd    : The wishbone descriptor read from wishbone address space
 * @driver : The driver managing this device
 * @list   : List of Wishbone devices (per bus)
 * @dev    : Internal Linux device structure
 */
struct wb_device {
	struct sdwb_wbd wbd;
	struct wb_driver *driver;
	struct list_head list;
	struct device dev;
	struct wb_bus *bus;
};
#define to_wb_device(dev) container_of(dev, struct wb_device, dev);

struct wb_ops {
	int (*read8)(uint64_t, uint8_t *);
	int (*read16)(uint64_t, uint16_t *);
	int (*read32)(uint64_t, uint32_t *);
	int (*read64)(uint64_t, uint64_t *);

	int (*write8)(uint64_t, uint8_t);
	int (*write16)(uint64_t, uint16_t);
	int (*write32)(uint64_t, uint32_t);
	int (*write64)(uint64_t, uint64_t);

	int (*memcpy_from_wb) (uint64_t addr, size_t len, size_t *retlen,
		uint8_t *buf);
	int (*memcpy_to_wb) (uint64_t addr, size_t len, size_t *retlen,
		const uint8_t *buf);
};

struct wb_bus {
	char *name;
	struct module *owner;
	uint64_t sdwb_header_base;
	struct wb_ops *ops;
	struct list_head devices;
	struct mutex dev_lock;
	int ndev;
};

int wb_register_driver(struct wb_driver *driver);
void wb_unregister_driver(struct wb_driver *driver);

int wb_register_bus(struct wb_bus *bus);
void wb_unregister_bus(struct wb_bus *bus);

int wb_register_device(struct wb_device *wbdev);
void wb_unregister_device(struct wb_device *wbdev);

/* Wishbone I/O operations */
int wb_read8(struct wb_bus *, uint64_t, uint8_t *);
int wb_read16(struct wb_bus *, uint64_t, uint16_t *);
int wb_read32(struct wb_bus *, uint64_t, uint32_t *);
int wb_read64(struct wb_bus *, uint64_t, uint64_t *);

int wb_write8(struct wb_bus *, uint64_t, uint8_t);
int wb_write16(struct wb_bus *, uint64_t, uint16_t);
int wb_write32(struct wb_bus *, uint64_t, uint32_t);
int wb_write64(struct wb_bus *, uint64_t, uint64_t);

int memcpy_from_wb(struct wb_bus *, uint64_t addr, size_t len,
	uint8_t *buf);
int memcpy_to_wb(struct wb_bus *, uint64_t addr, size_t len,
	const uint8_t *buf);

#endif /* _LINUX_WISHBONE_H */

