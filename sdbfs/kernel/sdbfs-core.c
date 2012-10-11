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
#include <linux/uaccess.h>

#include "sdbfs.h"
#include "sdbfs-int.h"

#define SDB_SIZE (sizeof(struct sdb_device))

static void sdbfs_fix_endian(struct sdbfs_dev *sd, void *ptr, int len)
{
	uint32_t *p = ptr;
	int i;

	if (!(sd->flags & SDBFS_F_FIXENDIAN))
		return;
	if (len & 3)
		return; /* Hmmm... */

	for (i = 0; i < len / 4; i++)
		p[i] = htonl(p[i]);
}

static int sdbfs_readdir(struct file * filp,
			 void * dirent, filldir_t filldir)
{
	struct inode *ino = filp->f_dentry->d_inode;
	struct sdbfs_inode *inode;
	struct sdbfs_info *info;
	unsigned long file_off = filp->f_pos;
	unsigned long offset = ino->i_ino & ~1;
	int i, n;

	printk("%s\n", __func__);

	inode = container_of(ino, struct sdbfs_inode, ino);
	n = inode->nfiles;

	for (i = file_off; i < n; i++) {

		offset += SDB_SIZE; /* new inode number */
		info = inode->files + i;
		if (filldir(dirent, info->name, info->namelen,
			    offset, offset, DT_UNKNOWN) < 0)
			return i;
		filp->f_pos++;
	}
	return i;
}

static const struct file_operations sdbfs_dir_fops = {
	.read		= generic_read_dir,
	.readdir	= sdbfs_readdir,
	.llseek		= default_llseek,
};

static ssize_t sdbfs_read(struct file *f, char __user *buf, size_t count,
			  loff_t *offp)
{
	struct inode *ino = f->f_dentry->d_inode;
	struct super_block *sb = ino->i_sb;
	struct sdbfs_dev *sd = sb->s_fs_info;
	struct sdbfs_inode *inode;
	char kbuf[16];
	unsigned long start, size;
	ssize_t i, done;

	inode = container_of(ino, struct sdbfs_inode, ino);
	start = be64_to_cpu(inode->info.s_d.sdb_component.addr_first);
	size = be64_to_cpu(inode->info.s_d.sdb_component.addr_last) + 1 - start;

	if (*offp > size)
		return 0;
	if (*offp + count > size)
		count = size - *offp;
	done = 0;
	while (done < count) {
		/* Horribly inefficient, just copy a few bytes at a time */
		int n = sizeof(kbuf) > count ? count : sizeof(kbuf);

		/* FIXME: error checking */
		i = sd->ops->read(sd, start + *offp, kbuf, n);
		if (i < 0) {
			if (done)
				return done;
			return i;
		}
		if (copy_to_user(buf, kbuf, i))
			return -EFAULT;
		buf += i;
		done += i;
		if (i != n) {
			/* Partial read: done for this time */
			break;
		}
	}
	*offp += done;
	return done;
}

static const struct file_operations sdbfs_fops = {
	.read		= sdbfs_read,
};
static struct inode *sdbfs_iget(struct super_block *sb, int inum);

static struct dentry *sdbfs_lookup(struct inode *dir,
				   struct dentry *dentry, struct nameidata *nd)
{
	struct inode *ino = NULL;
	struct sdbfs_inode *inode = container_of(dir, struct sdbfs_inode, ino);
	struct sdbfs_info *info;
	unsigned long inum = dir->i_ino & ~1;
	int i, n, len;

	n = inode->nfiles;
	len = dentry->d_name.len;
	for (i = 0; i < n; i++) {
		info = inode->files + i;
		if (info->namelen != len)
			continue;
		if (!strncmp(info->name, dentry->d_name.name, len))
			break;
	}
	if (i != n)
		ino = sdbfs_iget(dir->i_sb, inum + SDB_SIZE * (i+1));
	d_add(dentry, ino);
	return 0;
}

static const struct inode_operations sdbfs_dir_iops = {
	.lookup		= sdbfs_lookup,
};

static struct kmem_cache *sdbfs_inode_cache;

static struct inode *sdbfs_alloc_inode(struct super_block *sb)
{
	struct sdbfs_inode *inode;

	printk("%s\n", __func__);
	inode = kmem_cache_alloc(sdbfs_inode_cache, GFP_KERNEL);
	if (!inode)
		return NULL;
	inode_init_once(&inode->ino);
	printk("%s: return %p\n", __func__, &inode->ino);
	return &inode->ino;
}

static void sdbfs_destroy_inode(struct inode *ino)
{
	struct sdbfs_inode *inode;

	inode = container_of(ino, struct sdbfs_inode, ino);
	kfree(inode->files);
	kmem_cache_free(sdbfs_inode_cache, inode);
}

static const struct super_operations sdbfs_super_ops = {
	.alloc_inode    = sdbfs_alloc_inode,
	.destroy_inode  = sdbfs_destroy_inode,
};

static struct inode *sdbfs_iget(struct super_block *sb, int inum)
{
	struct inode *ino;
	struct sdbfs_dev *sd = sb->s_fs_info;
	struct sdbfs_inode *inode;
	struct sdbfs_info *info;
	uint32_t offset;
	int i, j, n, len;
	int type;

	printk("%s: inum %i\n", __func__, inum);
	ino = iget_locked(sb, inum);
	if (!ino)
		return ERR_PTR(-ENOMEM);
	if (!(ino->i_state & I_NEW))
		return ino;

	/* The inum is the offset, but to avoid 0 we set the LSB */
	offset = inum & ~1;
	inode = container_of(ino, struct sdbfs_inode, ino);

	n = sd->ops->read(sd, offset, &inode->info.s_d, SDB_SIZE);
	if (n != SDB_SIZE)
		return ERR_PTR(-EIO);
	sdbfs_fix_endian(sd, &inode->info.s_d, SDB_SIZE);

	set_nlink(ino, 1);
	ino->i_size = be64_to_cpu(inode->info.s_d.sdb_component.addr_last)
		- be64_to_cpu(inode->info.s_d.sdb_component.addr_first) + 1;
	ino->i_mtime.tv_sec = ino->i_atime.tv_sec = ino->i_ctime.tv_sec = 0;
	ino->i_mtime.tv_nsec = ino->i_atime.tv_nsec = ino->i_ctime.tv_nsec = 0;

	type = inode->info.s_d.sdb_component.product.record_type;
	switch (type) {
	case sdb_type_interconnect:
		printk("%s: interconnect\n", __func__);
		ino->i_mode = S_IFDIR  | 0555;
		ino->i_op = &sdbfs_dir_iops;
		ino->i_fop = &sdbfs_dir_fops;
		/* We are an interconnect, so read the other records too */
		inode->nfiles = be16_to_cpu(inode->info.s_i.sdb_records);
		len = inode->nfiles * sizeof(*info);
		inode->files = kzalloc(len, GFP_KERNEL);
		if (!inode->files) {
			/* FIXME: iput? */
			return ERR_PTR(-ENOMEM);
		}

		/* FIXME: add dot and dotdot, and maybe named-dot */
		for (i = 0; i < inode->nfiles; i++) {
			offset += SDB_SIZE;
			info = inode->files + i;
			n = sd->ops->read(sd, offset, &info->s_d, SDB_SIZE);
			if (n != SDB_SIZE) {
				/* FIXME: iput? */
				kfree(inode->files);
				return ERR_PTR(-EIO);
			}
			sdbfs_fix_endian(sd, &info->s_d, SDB_SIZE);
			strncpy(info->name,
				info->s_d.sdb_component.product.name, 19);
			for (j = 19; j; j--) {
				info->name[j] = '\0';
				if (info->name[j-1] != ' ')
					break;
			}
			info->namelen = j;
		}
		break;

	case sdb_type_device:
		printk("%s: device\n", __func__);
		/* FIXME: Which bus type is this? */
		ino->i_mode = S_IFREG | 0444;
		ino->i_fop = &sdbfs_fops;
		break;

	case sdb_type_bridge:
		printk("%s: bridge to %llx (unsupported yet)\n", __func__,
		       ntohll(inode->info.s_b.sdb_child));
		break;

	default:
		if (type & 0x80) /* informative only */
			pr_info("%s: ignoring unknown  record 0x%02x\n",
				__func__, type);
		else
			pr_err("%s: unsupported record 0x%02x\n",
			       __func__, type);
		break;
	}
	unlock_new_inode(ino);
	return ino;
}

static int sdbfs_fill_super(struct super_block *sb, void *data, int silent)
{
	struct inode *inode;
	struct dentry *root;
	struct sdbfs_dev *sd;
	uint32_t magic;

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

	inode = sdbfs_iget(sb, sd->entrypoint + 1 /* can't be zero */);
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

static int sdbfs_init(void)
{
	sdbfs_inode_cache = KMEM_CACHE(sdbfs_inode, 0);
	if (!sdbfs_inode_cache)
		return -ENOMEM;
	return register_filesystem(&sdbfs_fs_type);
	return 0;
}

static void sdbfs_exit(void)
{
	unregister_filesystem(&sdbfs_fs_type);
	kmem_cache_destroy(sdbfs_inode_cache);
}

module_init(sdbfs_init);
module_exit(sdbfs_exit);

MODULE_LICENSE("GPL");
