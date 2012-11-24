/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "sdbfs-int.h"

static ssize_t sdbfs_read(struct file *f, char __user *buf, size_t count,
			  loff_t *offp)
{
	struct inode *ino = f->f_dentry->d_inode;
	struct sdbfs_dev *sd;
	struct sdbfs_inode *inode;
	char kbuf[16];
	unsigned long start, size;
	ssize_t i, done;

	inode = container_of(ino, struct sdbfs_inode, ino);
	sd = inode->sd;
	start = be64_to_cpu(inode->info.s_d.sdb_component.addr_first);
	size = be64_to_cpu(inode->info.s_d.sdb_component.addr_last) + 1 - start;

	if (*offp > size)
		return 0;
	if (*offp + count > size)
		count = size - *offp;
	done = 0;
	start += *offp;
	while (count) {
		/* Horribly inefficient, just copy a few bytes at a time */
		int n = sizeof(kbuf) > count ? count : sizeof(kbuf);

		/* FIXME: error checking */
		i = sd->ops->read(sd, start + done, kbuf, n);
		if (i < 0) {
			if (done)
				return done;
			return i;
		}
		if (copy_to_user(buf, kbuf, i))
			return -EFAULT;
		buf += i;
		done += i;
		count -= i;
		if (i != n) {
			/* Partial read: done for this time */
			break;
		}
	}
	*offp += done;
	return done;
}

const struct file_operations sdbfs_fops = {
	.read		= sdbfs_read,
};
