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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/sdb.h> /* in ../include, by now */

#include "sdbfs-int.h"

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

/* This is called by readdir and by lookup, when needed */
static int sdbfs_read_whole_dir(struct sdbfs_inode *inode)
{
	struct sdbfs_info *info;
	struct sdbfs_dev *sd = inode->sd;
	unsigned long offset;
	int i, j, n;

	printk("%s -- %i\n", __func__, inode->nfiles);
	if (inode->nfiles)
		return 0;

	printk("%s\n", __func__);

	/* Get the interconnect and see how many */
	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	n = sd->ops->read(sd, inode->base_sdb, &info->s_i, SDB_SIZE);
	if (n != SDB_SIZE) {
		kfree(info);
		return -EIO;
	}
	sdbfs_fix_endian(sd, &info->s_i, SDB_SIZE);
	printk("read at offset %li\n", inode->base_sdb);

	if (info->s_i.sdb_magic != htonl(SDB_MAGIC)) {
		pr_err("%s: wrong magic (%08x) at offset 0x%lx\n", __func__,
		       info->s_i.sdb_magic, inode->base_sdb);
		kfree(info);
		return -EINVAL;
	}
	inode->nfiles = be16_to_cpu(info->s_i.sdb_records);
	kfree(info);

	printk("nfiles %i\n", inode->nfiles);

	info = kmalloc(sizeof(*info) * inode->nfiles, GFP_KERNEL);
	if (!info) {
		inode->nfiles = 0;
		return -ENOMEM;
	}

	offset = inode->base_sdb;
	printk("reading at 0x%lx\n", offset);

	inode->files = info;
	for (i = 0; i < inode->nfiles; i++) {
		info = inode->files + i;
		n = sd->ops->read(sd, offset, &info->s_d, SDB_SIZE);
		if (n != SDB_SIZE) {
			/* FIXME: iput? */
			kfree(inode->files);
			inode->nfiles = 0;
			return -EIO;
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
		offset += SDB_SIZE;
	}
	return 0;
}

static int sdbfs_readdir(struct file * filp,
			 void * dirent, filldir_t filldir)
{
	struct inode *ino = filp->f_dentry->d_inode;
	struct sdbfs_inode *inode;
	struct sdbfs_info *info;
	unsigned long offset;
	int i, type, done = 0;

	printk("%s\n", __func__);

	/* dot and dotdot are special */
	if (filp->f_pos == 0) {
		if (filldir(dirent, ".", 1, 0, ino->i_ino, DT_DIR) < 0)
			return done;
		done++;
		filp->f_pos++;
	}
	if (filp->f_pos == 1) {
		if (filldir(dirent, "..", 2, 1,
			    parent_ino(filp->f_dentry), DT_DIR) < 0)
			return done;
		done++;
		filp->f_pos++;
	}

	/* Then our stuff */
	printk("%s: %i -- inum %li\n", __func__, __LINE__, ino->i_ino);
	inode = container_of(ino, struct sdbfs_inode, ino);
	sdbfs_read_whole_dir(inode);
	offset = inode->base_sdb;

	for (i = filp->f_pos - 2; i < inode->nfiles; i++) {
		info = inode->files + i;
		if (info->s_e.record_type == sdb_type_bridge)
			type = DT_DIR;
		else
			type = DT_REG;

		if (filldir(dirent, info->name, info->namelen,
			    SDBFS_INO(inode->sd, offset),
			    i + 2 /* ? */, type) < 0)
			return done;
		filp->f_pos++;
		done++;
	}
	return done;
}

static const struct file_operations sdbfs_dir_fops = {
	.read		= generic_read_dir,
	.readdir	= sdbfs_readdir,
	.llseek		= default_llseek,
};

static struct dentry *sdbfs_lookup(struct inode *dir,
				   struct dentry *dentry, struct nameidata *nd)
{
	struct inode *ino = NULL;
	struct sdbfs_inode *inode = container_of(dir, struct sdbfs_inode, ino);
	struct sdbfs_info *info;
	unsigned long offset = inode->base_sdb;
	int i, n, len;

	printk("%s\n", __func__);
	sdbfs_read_whole_dir(inode);
	n = inode->nfiles;
	len = dentry->d_name.len;
	for (i = 0; i < n; i++) {
		info = inode->files + i;
		if (info->namelen != len)
			continue;
		if (!strncmp(info->name, dentry->d_name.name, len))
			break;
	}
	if (i != n) {
		offset = offset + SDB_SIZE * i;
		printk("lookup in inum 0x%lx, sd %p\n", dir->i_ino, inode->sd);
		ino = sdbfs_iget(inode, dir->i_sb,
				 SDBFS_INO(inode->sd, offset), inode->sd);
	}
	d_add(dentry, ino);
	return 0;
}

static const struct inode_operations sdbfs_dir_iops = {
	.lookup		= sdbfs_lookup,
};


struct inode *sdbfs_alloc_inode(struct super_block *sb)
{
	struct sdbfs_inode *inode;

	printk("%s\n", __func__);
	inode = kmem_cache_alloc(sdbfs_inode_cache, GFP_KERNEL);
	if (!inode)
		return NULL;
	inode_init_once(&inode->ino);
	inode->files = NULL;
	if (0)
		dump_stack();
	return &inode->ino;
}

void sdbfs_destroy_inode(struct inode *ino)
{
	struct sdbfs_inode *inode;

	printk("%s\n", __func__);
	inode = container_of(ino, struct sdbfs_inode, ino);
	kfree(inode->files);
	kmem_cache_free(sdbfs_inode_cache, inode);
}

static struct inode *sdbfs_iget_rootdev(struct super_block *sb,
					struct inode *ino,
					struct sdbfs_dev *sd)
{
	struct sdbfs_inode *inode = container_of(ino, struct sdbfs_inode, ino);
	struct sdb_bridge *b = &inode->info.s_b;

	inode->sd = sd;

	/* The root directory is a fake "bridge" structure */
	memset(b, 0, sizeof(*b));
	b->sdb_child = cpu_to_be64(sd->entrypoint);
	b->sdb_component.addr_first = 0;
	b->sdb_component.addr_last = cpu_to_be64(sd->size);
	b->sdb_component.product.record_type = sdb_type_bridge;

	/* So, this is a directory, and it links to the first interconnect */
	printk("%s inum 0x%lx -- size %li\n", __func__, ino->i_ino, sd->size);
	inode->base_data = 0;
	inode->base_sdb = sd->entrypoint;
	inode->nfiles = 0;
	ino->i_size = sd->size;
	ino->i_mode = S_IFDIR  | 0555;
	ino->i_op = &sdbfs_dir_iops;
	ino->i_fop = &sdbfs_dir_fops;

	unlock_new_inode(ino);
	return ino;
}

struct inode *sdbfs_iget(struct sdbfs_inode *parent,
			 struct super_block *sb, unsigned long inum,
			 struct sdbfs_dev *sd)
{
	struct inode *ino;
	struct sdbfs_inode *inode;
	uint32_t offset;
	unsigned long size, base_data; /* target offset */
	int n;
	int type;

	printk("%s: inum 0x%lx\n", __func__, inum);
	ino = iget_locked(sb, inum);
	if (!ino)
		return ERR_PTR(-ENOMEM);
	if (!(ino->i_state & I_NEW))
		return ino;

	/* 1 link because the structure is immutable, who cares */
	set_nlink(ino, 1);
	ino->i_mtime.tv_sec = ino->i_atime.tv_sec = ino->i_ctime.tv_sec = 0;
	ino->i_mtime.tv_nsec = ino->i_atime.tv_nsec = ino->i_ctime.tv_nsec = 0;

	/* Two special cases: root and root of a device */
	if (unlikely(SDBFS_IS_ROOT(inum)))
		return sdbfs_iget_root(sb, ino);
	if (unlikely(SDBFS_IS_FIRST(inum)))
		return sdbfs_iget_rootdev(sb, ino, sd);

	/* Just a child node, the parent passed sd already */
	inode = container_of(ino, struct sdbfs_inode, ino);
	inode->sd = sd;
	offset = SDBFS_OFFSET(sd, inum);

	n = sd->ops->read(sd, offset, &inode->info.s_d, SDB_SIZE);
	if (n != SDB_SIZE)
		return ERR_PTR(-EIO);
	sdbfs_fix_endian(sd, &inode->info.s_d, SDB_SIZE);


	base_data = be64_to_cpu(inode->info.s_d.sdb_component.addr_first);
	size = be64_to_cpu(inode->info.s_d.sdb_component.addr_last)
		- base_data + 1;

	type = inode->info.s_e.record_type;
	switch (type) {
	case sdb_type_interconnect:
	case sdb_type_device:
		/* You can access internal registers/data */
		ino->i_fop = &sdbfs_fops; /* FIXME: Which bus type? */
		ino->i_mode = S_IFREG | 0444;
		ino->i_size = size;
		inode->base_data = parent->base_data + base_data;
		break;

	case sdb_type_bridge:
		/* A bridge is a subdirectory */
		ino->i_mode = S_IFDIR  | 0555;
		ino->i_op = &sdbfs_dir_iops;
		ino->i_fop = &sdbfs_dir_fops;
		inode->base_data = parent->base_data + base_data;
		inode->base_sdb = parent->base_data
			+ be64_to_cpu(inode->info.s_b.sdb_child);
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
