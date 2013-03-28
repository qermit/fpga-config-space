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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sdb.h> /* in ../include, by now */

#include "sdbfs-int.h"

static const struct super_operations sdbfs_super_ops = {
	.alloc_inode    = sdbfs_alloc_inode,
	.destroy_inode  = sdbfs_destroy_inode,
};

static int sdbfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;
	struct sdbfs_dev *sd;
	uint32_t magic;

	printk("%s\n", __func__);

	/* HACK: this data is really a name */
	sd = sdbfs_get_by_name(data);
	if (IS_ERR(sd))
		return PTR_ERR(sd);
	sb->s_fs_info = sd;

	/* Check magic number first */
	sd->ops->read(sd, sd->entrypoint, &magic, 4);
	if (magic == ntohl(SDB_MAGIC)) {
		/* all right: we are big endian or byte-level connected */
	} else if (magic == SDB_MAGIC) {
		/* looks like we are little-endian on a 32-bit-only bus */
		sd->flags |= SDBFS_F_FIXENDIAN;
	} else {
		printk("%s: wrong magic at 0x%lx (%08x is not %08x)\n",
		       __func__, sd->entrypoint, magic, SDB_MAGIC);
		return -EINVAL;
	}

	/* All of our data is organized as 64-byte blocks */
	sb->s_blocksize = 64;
	sb->s_blocksize_bits = 6;
	sb->s_magic = SDB_MAGIC;
	sb->s_op = &sdbfs_super_ops;

	/* The root inode is 1. It is a fake bridge and has no parent. */
	inode = sdbfs_iget(NULL, sb, SDBFS_ROOT);
	if (IS_ERR(inode)) {
		sdbfs_put(sd);
		return PTR_ERR(inode);
	}

	/*
	 * Instantiate and link root dentry. d_make_root only exists
	 * after 3.2, but d_alloc_root was killed soon after 3.3
	 */
	root = d_make_root(inode);
	if (!root) {
		sdbfs_put(sd);
		/* FIXME: release inode? */
		return -ENOMEM;
	}
	root->d_fsdata = NULL; /* FIXME: d_fsdata */
	sb->s_root = root;
	return 0;
}

static struct dentry *sdbfs_mount(struct file_system_type *type, int flags,
			   const char *name, void *data)
{
	struct dentry *ret;
	char *fakedata = (char *)name;

	/* HACK: use "name" as data, to use the mount_single helper */
	ret = mount_single(type, flags, fakedata, sdbfs_fill_super);
	printk("%s: %p\n", __func__, ret);
	return ret;
}

static void sdbfs_kill_sb(struct super_block *sb)
{
	struct sdbfs_dev *sd = sb->s_fs_info;

	printk("%s\n", __func__);
	kill_anon_super(sb);
	if (sd)
		sdbfs_put(sd);
}

static struct file_system_type sdbfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "sdbfs",
	.mount		= sdbfs_mount, /* 2.6.37 and later only */
	.kill_sb	= sdbfs_kill_sb,
};

struct kmem_cache *sdbfs_inode_cache;

static int __init sdbfs_init(void)
{
	sdbfs_inode_cache = KMEM_CACHE(sdbfs_inode, 0);
	if (!sdbfs_inode_cache)
		return -ENOMEM;
	return register_filesystem(&sdbfs_fs_type);
	return 0;
}

static void __exit sdbfs_exit(void)
{
	unregister_filesystem(&sdbfs_fs_type);
	kmem_cache_destroy(sdbfs_inode_cache);
}

module_init(sdbfs_init);
module_exit(sdbfs_exit);

MODULE_LICENSE("GPL");
