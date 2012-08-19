/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __SDBFS_H__
#define __SDBFS_H__

struct sdbfs_dev;

/*
 * Client modules register sdbfs-capable devices, and several of them
 * may use the same hardware abstraction, so separate it. We use 32-bit
 * addresses, being aware of potential issues, but our target is small
 * devices accessed by a soft-core. If the need arises, we'll add erase64
 * and similar methods.
 */
struct sdbfs_dev_ops {
	struct module *owner;
	int (*erase)(struct sdbfs_dev *sd, uint32_t begin, uint32_t end);
	ssize_t (*read)(struct sdbfs_dev *sd, uint32_t begin, void *buf,
			size_t count);
	ssize_t (*write)(struct sdbfs_dev *sd, uint32_t begin, void *buf,
			 size_t count);
};

struct sdbfs_dev {
	char *name;
	int blocksize;
	unsigned long headerpos;
	struct sdbfs_dev_ops *ops;
	struct list_head list;
};


/* Internal inter-file calls */
struct sdbfs_dev *sdbfs_get_by_name(char *name);
void sdbfs_put(struct sdbfs_dev *sd);

/* Exported to other modules */
int sdbfs_register_device(struct sdbfs_dev *sd);
void sdbfs_unregister_device(struct sdbfs_dev *sd);

#endif /* __SDBFS_H__ */
