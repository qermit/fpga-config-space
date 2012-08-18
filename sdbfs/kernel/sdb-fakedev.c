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
#include <linux/device.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include "sdbfs.h"

/* We register up to 8 filesystems getting the names as module parameters */
static int nimg;
static char *fsimg[8];
module_param_array(fsimg, charp, &nimg, 0444);

struct fakedev {
	struct sdbfs_dev sd;
	const struct firmware *fw;
};

static struct fakedev fakedev_devs[8];
static struct device fakedev_device;

static ssize_t fakedev_read(struct sdbfs_dev *sd, uint32_t begin, u8 *buf,
		     size_t count)
{
	struct fakedev *fd;
	int len;

	fd = container_of(sd, struct fakedev, sd);
	len = fd->fw->size;
	if (begin > len)
		return -EINVAL;
	if (begin + count > len)
		count = len - begin;
	memcpy(buf, fd->fw->data + begin, count);
	return count;
}

static struct sdbfs_dev_ops fakedev_ops = {
	.owner = THIS_MODULE,
	.erase = NULL,
	.read = fakedev_read,
	.write = NULL,
};


static int fakedev_init(void)
{
	struct fakedev *d;
	int i;

	/* we need a device to request a firmware image */
	dev_set_name(&fakedev_device, "sdbfs-fakedev");
	device_initialize(&fakedev_device);
	i = device_add(&fakedev_device);
	if (i < 0) {
		printk("%s: failed to init device (error %i)\n",
		       KBUILD_MODNAME, i);
		return i;
	}

	for (i = 0; i < nimg; i++) {
		d = fakedev_devs + i;
		if (request_firmware(&d->fw, fsimg[i], &fakedev_device) < 0) {
			printk("%s: can't load %s\n", KBUILD_MODNAME,
			       fsimg[i]);
			continue;
		}
		d->sd.name = fsimg[i];
		d->sd.blocksize = 64; /* bah! */
		d->sd.ops = &fakedev_ops;
		if (sdbfs_register_device(&d->sd) < 0) {
			printk("%s: can't register %s\n", KBUILD_MODNAME,
			       fsimg[i]);
			release_firmware(d->fw);
			d->fw = NULL;
		}
	}
	return 0;
}

static void fakedev_exit(void)
{
	struct fakedev *d;
	int i;

	for (i = 0; i < nimg; i++) {
		d = fakedev_devs + i;
		if (!d->fw)
			continue;
		sdbfs_unregister_device(&d->sd);
		release_firmware(d->fw);
	}
	device_del(&fakedev_device);
}

module_init(fakedev_init);
module_exit(fakedev_exit);
