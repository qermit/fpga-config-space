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
#include <linux/sdb.h>
#include "sdbfs.h"

struct sdbmem {
	struct sdbfs_dev sd;
	void __iomem *address;
	size_t datalen;
	int ready;
};

/* We register up to 8 filesystems getting the description at insmod */
static int sdbmem_narea;

static char *sdbmem_area[8];
module_param_array_named(area, sdbmem_area, charp, &sdbmem_narea, 0444);

/*
 *   area=<name>@<address>-<address>[=<entrypoint>]
 *   area=<name>@<address>+<lenght>[=<entrypoint>]
 */

static int sdbmem_parse(char *desc, struct sdbmem *d)
{
	unsigned long addr, size;
	char *at;
	int i;
	char c;

	memset(d, 0, sizeof(*d));
	at = strchr(desc, '@');
	if (!at)
		return -EINVAL;
	i = sscanf(at,"@%lx+%lx=%lx%c", &addr, &size,
		   &d->sd.entrypoint, &c);
	if (i == 1) {
		i = sscanf(at,"@%lx-%lx=%lx%c", &addr, &size,
			   &d->sd.entrypoint, &c);
		size -= addr;
	}
	if (i < 2 || i > 3) {
		pr_err("%s: wrong argument \"%s\"\n", KBUILD_MODNAME,
			 desc);
		pr_err("Use \"<name>@<addr>[-+]<addr>[=<entrypoint>\"\n");
		return -EINVAL;
	}
	/* So, the name is the first one and there is the '@' sign */
	*at = '\0';
	d->sd.name = desc;
	d->address = ioremap(addr, size);
	if (!d->address)
		return -ENOMEM;
	d->sd.size = d->datalen = size;
	return 0;
}

static struct sdbmem sdbmem_devs[8];

static ssize_t sdbmem_read(struct sdbfs_dev *sd, uint32_t begin, void *buf,
		     size_t count)
{
	struct sdbmem *fd;
	size_t len;

	printk("%s: %08x - %i\n", __func__, (int)begin, count);
	fd = container_of(sd, struct sdbmem, sd);
	len = fd->datalen;
	if (begin > len)
		return -EINVAL;
	if (begin + count > len)
		count = len - begin;
	memcpy_fromio(buf, fd->address + begin, count);
	return count;
}

static ssize_t sdbmem_write(struct sdbfs_dev *sd, uint32_t begin,
			    const void *buf, size_t count)
{
	struct sdbmem *fd;
	size_t len;

	printk("%s: %08x - %i\n", __func__, (int)begin, count);
	fd = container_of(sd, struct sdbmem, sd);
	len = fd->datalen;
	if (begin > len)
		return -EINVAL;
	if (begin + count > len)
		count = len - begin;
	memcpy_toio(fd->address + begin, buf, count);
	return count;
}


static struct sdbfs_dev_ops sdbmem_ops = {
	.owner = THIS_MODULE,
	.erase = NULL,
	.read = sdbmem_read,
	.write = NULL,
};

/* FIXME: export the register and unregister functions for external users */

static int sdbmem_init(void)
{
	struct sdbmem *d;
	int i, done = 0;
	uint32_t magic;

	for (i = 0; i < sdbmem_narea; i++) {
		d = sdbmem_devs + i;
		if (sdbmem_parse(sdbmem_area[i], d) < 0)
			continue;

		/* Check the magic number, so we fail ASAP */
		magic = readl(d->address + d->sd.entrypoint);
		if (magic != SDB_MAGIC && magic != ntohl(SDB_MAGIC)) {
			iounmap(d->address);
			pr_err("%s: wrong magic 0x%08x at 0x%lx\n", __func__,
			       magic, d->sd.entrypoint);
			continue;
		}
		d->sd.blocksize = 4; /* bah! */
		d->sd.ops = &sdbmem_ops;
		if (sdbfs_register_device(&d->sd) < 0) {
			pr_err("%s: can't register area %s\n", KBUILD_MODNAME,
			       d->sd.name);
			continue;
		}
		done++;
		d->ready = 1;
	}
	if (done)
		return 0;
	return -ENODEV;
}

static void sdbmem_exit(void)
{
	struct sdbmem *d;
	int i;

	for (i = 0; i < sdbmem_narea; i++) {
		d = sdbmem_devs + i;
		if (!d->ready)
			continue;
		sdbfs_unregister_device(&d->sd);
		iounmap(d->address);
	}
}

module_init(sdbmem_init);
module_exit(sdbmem_exit);

MODULE_LICENSE("GPL");
