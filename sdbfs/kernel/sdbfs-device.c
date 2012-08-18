/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/sdb.h> /* in ../include, by now */

#include "sdbfs.h"
#include "sdbfs-int.h"

static LIST_HEAD(sdbfs_devlist);

struct sdbfs_dev *sdbfs_get_by_name(char *name)
{
	struct sdbfs_dev *sd;

	list_for_each_entry(sd, &sdbfs_devlist, list)
		if (!strcmp(sd->name, name))
			goto found;
	return ERR_PTR(-ENOENT);
found:
	if (try_module_get(sd->ops->owner)) {
		printk("%s: %p\n", __func__, sd);
		return sd;
	}
	return ERR_PTR(-ENOENT);
}

void sdbfs_put(struct sdbfs_dev *sd)
{
	printk("%s: %p\n", __func__, sd);
	module_put(sd->ops->owner);
}

/* Exported functions */

int sdbfs_register_device(struct sdbfs_dev *sd)
{
	struct sdbfs_dev *osd;

	list_for_each_entry(osd, &sdbfs_devlist, list)
		if (!strcmp(osd->name, sd->name))
			return -EBUSY;
	list_add(&sd->list, &sdbfs_devlist);
	return 0;
}
EXPORT_SYMBOL(sdbfs_register_device);

void sdbfs_unregister_device(struct sdbfs_dev *sd)
{
	struct sdbfs_dev *osd;

	list_for_each_entry(osd, &sdbfs_devlist, list)
		if (osd == sd)
			list_del(&sd->list);
}
EXPORT_SYMBOL(sdbfs_unregister_device);
