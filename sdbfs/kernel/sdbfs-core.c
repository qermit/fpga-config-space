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
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sdb.h> /* in ../include, by now */

#include "sdbfs.h"
#include "sdbfs-int.h"


static int sdbfs_readdir(struct file * filp,
			 void * dirent, filldir_t filldir)
{
	printk("%s\n", __func__);
	return -ENOENT;
}

static const struct file_operations sdbfs_dir_operations = {
	.read		= generic_read_dir,
	.readdir	= sdbfs_readdir,
	.llseek		= default_llseek,
};

static struct dentry *sdbfs_lookup(struct inode *dir,
				   struct dentry *dentry, struct nameidata *nd)
{
	printk("%s\n", __func__);
	return NULL;
}

static const struct inode_operations sdbfs_dir_inode_operations = {
	.lookup		= sdbfs_lookup,
};

static struct inode *sdbfs_alloc_inode(struct super_block *sb)
{
	printk("%s\n", __func__);
	return NULL;
}

static void sdbfs_destroy_inode(struct inode *ino)
{
}

static const struct super_operations sdbfs_super_ops = {
	.alloc_inode    = sdbfs_alloc_inode,
	.destroy_inode  = sdbfs_destroy_inode,
};

static int sdbfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;

	/* All of our data is organized as 64-byte blocks */
	sb->s_blocksize = 64;
	sb->s_blocksize_bits = 6;
	sb->s_magic = SDB_MAGIC;
	sb->s_op = &sdbfs_super_ops;

	inode = iget_locked(sb, 1 /* FIXME: inode number */);
	if (!inode) {
		printk("no inode\n");
		return -ENOMEM;
	}

	/* instantiate and link root dentry */
	root = d_alloc_root(inode);
	if (!root) {
		iput(inode);
		return -ENOMEM;
	}
	root->d_fsdata = NULL; /* FIXME: d_fsdata */
	sb->s_root = root;
	return 0;
}

static struct dentry *sdbfs_mount(struct file_system_type *type, int flags,
			   const char *name, void *data)
{
	/* FIXME: use "name" */
	printk("%s: flags 0x%x, name %s, data %p\n", __func__,
	       flags, name, data);
	return mount_single(type, flags, data, sdbfs_fill_super);
}

static void sdbfs_kill_sb(struct super_block *sb)
{
	struct sdbfs_dev *sd = sb->s_fs_info;

	kill_anon_super(sb);
	if (sd)
		module_put(sd->ops->owner);
}

static struct file_system_type sdbfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "sdbfs",
	.mount		= sdbfs_mount, /* 2.6.37 and later only */
	.kill_sb	= sdbfs_kill_sb,
};

static int sdbfs_init(void)
{
	return register_filesystem(&sdbfs_fs_type);
	return 0;
}

static void sdbfs_exit(void)
{
	unregister_filesystem(&sdbfs_fs_type);
}

module_init(sdbfs_init);
module_exit(sdbfs_exit);




