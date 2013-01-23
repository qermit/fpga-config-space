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

/*
 * We need to allocate inodes for the individual devices.
 * This is a crappy implementation that doesn't scale.
 */
static int sdbfs_alloc_inodes(struct sdbfs_dev *sd)
{
	static unsigned long lastino;
	unsigned long size;
	int ret;

	ret = lastino;
	size = (sd->size + 1023L) & ~1023L;
	lastino += size; /* FIXME: alloc inodes */
	printk("new dev: base is 0x%x (%i)\n", ret, ret);
	return ret;
}


static void sdbfs_free_inodes(struct sdbfs_dev *sd)
{
	/* FIXME: release inode numbers allocated above */
}


static LIST_HEAD(sdbfs_devlist);
static DEFINE_SPINLOCK(sdbfs_devlock);

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
	int ret = -EBUSY;
	uint32_t magic;

	/* Check magic number */
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

	/* Ok, then register the thing */
	INIT_LIST_HEAD(&sd->ino_list);
	spin_lock(&sdbfs_devlock);
	list_for_each_entry(osd, &sdbfs_devlist, list)
		if (!strcmp(osd->name, sd->name))
			goto out;
	ret = sdbfs_alloc_inodes(sd);
	if (ret < 0)
		goto out;
	sd->ino_base = ret;
	list_add(&sd->list, &sdbfs_devlist);
	spin_unlock(&sdbfs_devlock);
	printk("%s: device %s, ibase %li, size %li\n", __func__, sd->name,
	       sd->ino_base, sd->size);

	return 0;

out:
	spin_unlock(&sdbfs_devlock);
	return ret;
}
EXPORT_SYMBOL(sdbfs_register_device);

void sdbfs_unregister_device(struct sdbfs_dev *sd)
{
	struct sdbfs_dev *osd;

	printk("removing %p\n", sd);
	spin_lock(&sdbfs_devlock);
	list_for_each_entry(osd, &sdbfs_devlist, list)
		printk("current list entry %p\n", osd);
	list_for_each_entry(osd, &sdbfs_devlist, list)
		if (osd == sd)
			break;
	if (osd == sd) {
		list_del(&sd->list);
		/* FIXME: d_drop or d_put it all */
		sdbfs_free_inodes(sd);
	}
	spin_unlock(&sdbfs_devlock);
}
EXPORT_SYMBOL(sdbfs_unregister_device);

/*
 * The root directory is strictly related to clients, so here it is
 */

static int sdbfs_root_readdir(struct file * filp,
			      void * dirent, filldir_t filldir)
{
	struct inode *ino = filp->f_dentry->d_inode;
	struct sdbfs_dev *sd;
	unsigned long inum;
	int pos, done = 0;

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

	/* Then the list of all registered devices */
	pos = 2;
	spin_lock(&sdbfs_devlock);
	list_for_each_entry(sd, &sdbfs_devlist, list) {
		inum = sd->ino_base +  SDBFS_FIRST_INO(sd->entrypoint);
		if (filp->f_pos == pos) {
			if (filldir(dirent, sd->name, strlen(sd->name),
				    filp->f_pos, inum, DT_DIR) < 0)
				goto out;
			done++;
			filp->f_pos++;
		}
		pos++;
	}
	out:
	spin_unlock(&sdbfs_devlock);
	return done;
}

static struct inode *sdbfs_iget_fill_devroot(struct sdbfs_inode *parent,
					     struct super_block *sb,
					     unsigned long inum,
					     struct sdbfs_dev *sd)
{
	struct inode *ino;
	struct sdbfs_inode *inode;

	ino = sdbfs_iget(parent, sb, inum, sd);
	if (IS_ERR(ino))
		return ino;
	inode = container_of(ino, struct sdbfs_inode, ino);
	printk("%s: inum 0x%lx %li -> sd %p\n", __func__, inum, inum, sd);
	inode->sd = sd;

	/* FIXME: what about use count? */
	return ino;
}



static struct dentry *sdbfs_root_lookup(struct inode *dir,
					struct dentry *dentry,
					struct nameidata *nd)
{
	struct sdbfs_inode *inode;
	struct sdbfs_dev *sd;
	struct inode *ino = NULL;
	unsigned long inum;
	int len;

	printk("%s\n", __func__);

	inode = container_of(dir, struct sdbfs_inode, ino);
	spin_lock(&sdbfs_devlock);
	len = dentry->d_name.len;
	list_for_each_entry(sd, &sdbfs_devlist, list) {
		if (!strncmp(sd->name, dentry->d_name.name, len))
			break;
	}
	spin_unlock(&sdbfs_devlock);
	if (!strncmp(sd->name, dentry->d_name.name, len)) {
		inum = sd->ino_base +  SDBFS_FIRST_INO(sd->entrypoint);
		ino = sdbfs_iget_fill_devroot(inode, dir->i_sb, inum, sd);
		if (IS_ERR(ino))
			return (void *)ino;
	}
	d_add(dentry, ino);
	return 0;
}


static const struct file_operations sdbfs_rootdir_fops = {
	.read		= generic_read_dir,
	.readdir	= sdbfs_root_readdir,
	.llseek		= default_llseek,
};

static const struct inode_operations sdbfs_rootdir_iops = {
	.lookup		= sdbfs_root_lookup,
};

struct inode *sdbfs_iget_root(struct super_block *sb, struct inode *ino)
{
	/* No need to fill a fake bridge, as it's special anyways */
	ino->i_size = 0;
	ino->i_mode = S_IFDIR  | 0555;
	ino->i_op = &sdbfs_rootdir_iops;
	ino->i_fop = &sdbfs_rootdir_fops;

	unlock_new_inode(ino);
	return ino;
}
