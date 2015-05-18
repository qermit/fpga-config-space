/*
 * Copyright (C) 2015 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __LINUX_KSDB_H__
#define __LINUX_KSDB_H__

#include <linux/sdb.h>
/*
 * sdb, "self describing bus" is implemented in the register space of
 * FPGA images.  <linux/sdb.h> is the official header from the
 * specification.  To avoid name conflicts, everything here is kernel-sdb,
 * or "ksdb"
 */

#include <linux/fmc-sdb.h> /* We reuse structures defined there */

struct ksdb_device;
struct ksdb_driver;

/*
 * This bus abstraction is developed separately from drivers, so we need
 * to check the version of the data structures we receive.
 */
#define KSDB_MAJOR       1
#define KSDB_MINOR       0
#define KSDB_VERSION     ((SDB_MAJOR << 16) | SDB_MINOR)
#define __KSDB_MAJOR(x)  ((x) >> 16)
#define __KSDB_MINOR(x)  ((x) & 0xffff)

/*
 * Identification can be based on vendor/device, on class, abi and so on.
 * Some of this should be moved to mod_devicetable.h. The names and
 * values are modeled over USB.
 */
#define KSDB_MATCH_VENDOR		0x0001
#define KSDB_MATCH_PRODUCT		0x0002
#define KSDB_MATCH_ABI_LO		0x0004
#define KSDB_MATCH_ABI_HI		0x0008
#define KSDB_MATCH_ABI_CLASS		0x0010
#define KSDB_MATCH_VERSION_LO		0x0020
#define KSDB_MATCH_VERSION_HI		0x0040
#define KSDB_MATCH_BUSTYPE		0x0080

struct ksdb_device_id {
	/* What to match against */
	__u32 match_flags;
	__u64 vendor_id;
	__u32 device_id;
	__u32 version_lo;
	__u32 version_hi;
	__u16 abi_class;
	__u16 abi_lo;
	__u16 abi_hi;
	__u16 unused;
};

struct ksdb_driver {
	unsigned long version;
	struct device_driver driver;
	int (*probe)(struct ksdb_device *);
	int (*remove)(struct ksdb_device *);
	const struct ksdb_device_id id_table;
};

/*
 * Operations are registered by the "device" entity. SDB devices can
 * be memory based, I/O based, USB based or whatever
 */
struct ksdb_operations {
	/* FIXME: use lib-sdb instead? */
};

struct ksdb_device {
	unsigned long version;
	unsigned long flags;

	/* This is a copy of the 64-long in-ROM device description */
	union sdb_record record;

	unsigned long baseaddr;
	struct module *owner;
	struct ksdb_device_id *id_table;
	struct ksdb_operations *op;
};

/* pci-like naming */
static inline void *ksdb_get_drvdata(const struct ksdb_device *ksdb)
{
	return dev_get_drvdata(&ksdb->dev);
}

static inline void ksdb_set_drvdata(struct ksdb_device *ksdb, void *data)
{
	dev_set_drvdata(&ksdb->dev, data);
}

/* The 4 access points */
extern int ksdb_driver_register(struct ksdb_driver *drv);
extern void ksdb_driver_unregister(struct ksdb_driver *drv);
extern int ksdb_device_register(struct ksdb_device *tdev);
extern void ksdb_device_unregister(struct ksdb_device *tdev);

#endif /* __LINUX_KSDB_H__ */
