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



int sdbfs_register_device(struct sdbfs_dev *d)
{
	return -EAGAIN;
}
EXPORT_SYMBOL(sdbfs_register_device);

void sdbfs_unregister_device(struct sdbfs_dev *d)
{

}
EXPORT_SYMBOL(sdbfs_unregister_device);
