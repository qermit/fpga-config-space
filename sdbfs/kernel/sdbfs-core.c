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

	printk("%s\n", __func__);

	/* The root directory is actually just a placeholder by now */

	/* All of our data is organized as 64-byte blocks */
	sb->s_blocksize = 64;
	sb->s_blocksize_bits = 6;
	sb->s_magic = SDB_MAGIC;
	sb->s_op = &sdbfs_super_ops;

	/* The root inode is 1. It is a fake bridge and has no parent. */
	inode = sdbfs_iget(NULL, sb, SDBFS_ROOT, NULL);
	if (IS_ERR(inode))
		return PTR_ERR(inode);

	/*
	 * Instantiate and link root dentry. d_make_root only exists
	 * after 3.2, but d_alloc_root was killed soon after 3.3
	 */
	root = d_make_root(inode);
	if (!root) {
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

	ret = mount_single(type, flags, NULL, sdbfs_fill_super);
	printk("%s: %p\n", __func__, ret);
	return ret;
}

static struct file_system_type sdbfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "sdbfs",
	.mount		= sdbfs_mount, /* 2.6.37 and later only */
	.kill_sb	= kill_anon_super,
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
