/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __SDBFS_INT_H__
#define __SDBFS_INT_H__
#include <linux/fs.h>
#include <linux/sdb.h>

struct sdbfs_inode {
	union {
		struct sdb_device s_d;
		struct sdb_interconnect s_i;
		struct sdb_bridge s_b;
	};
	struct sdb_device *files; /* Only used for directories */
	struct inode ino;
};

#endif /* __SDBFS_INT_H__ */
