/*
 * Copyright (C) 2015 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/sdb.h>
#include <linux/ksdb.h>

static int sdb_probe(struct device *dev)
{
	struct sdb_driver *sdrv = to_sdb_driver(dev->driver);
	struct sdb_device *sdev = to_sdb_device(dev);

	return fdrv->probe(sdev);
}

static int sdb_remove(struct device *dev)
{
	struct sdb_driver *fdrv = to_sdb_driver(dev->driver);
	struct sdb_device *fdev = to_sdb_device(dev);

	return fdrv->remove(fdev);
}

static void sdb_shutdown(struct device *dev)
{
	/* not implemented but mandatory */
}

static struct bus_type sdb_bus_type = {
	.name = "sdb",
	.match = sdb_match,
	.uevent = sdb_uevent,
	.probe = sdb_probe,
	.remove = sdb_remove,
	.shutdown = sdb_shutdown,
};

static void sdb_release(struct device *dev)
{
	struct sdb_device *d = container_of(dev, struct sdb_device, dev);
	kfree(d);
}

/*
 * Functions for client modules follow
 */

int sdb_driver_register(struct sdb_driver *drv)
{
	if (sdb_check_version(drv->version, drv->driver.name))
		return -EINVAL;
	drv->driver.bus = &sdb_bus_type;
	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(sdb_driver_register);

void sdb_driver_unregister(struct sdb_driver *drv)
{
	driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(sdb_driver_unregister);

/*
 * When a device set is registered, all eeproms must be read
 * and all FRUs must be parsed
 */
int sdb_device_register_n(struct sdb_device **devs, int n)
{
	struct sdb_device *sdb, **devarray;
	uint32_t device_id;
	int i, ret = 0;

	if (n < 1)
		return 0;

	/* Check the version of the first data structure (function prints) */
	if (sdb_check_version(devs[0]->version, devs[0]->carrier_name))
		return -EINVAL;

	devarray = kmemdup(devs, n * sizeof(*devs), GFP_KERNEL);
	if (!devarray)
		return -ENOMEM;

	/* Make all other checks before continuing, for all devices */
	for (i = 0; i < n; i++) {
		sdb = devarray[i];
		if (!sdb->hwdev) {
			pr_err("%s: device nr. %i has no hwdev pointer\n",
			       __func__, i);
			ret = -EINVAL;
			break;
		}
		if (sdb->flags & FMC_DEVICE_NO_MEZZANINE) {
			dev_info(sdb->hwdev, "absent mezzanine in slot %d\n",
				 sdb->slot_id);
			continue;
		}
		if (!sdb->eeprom) {
			dev_err(sdb->hwdev, "no eeprom provided for slot %i\n",
				sdb->slot_id);
			ret = -EINVAL;
		}
		if (!sdb->eeprom_addr) {
			dev_err(sdb->hwdev, "no eeprom_addr for slot %i\n",
				sdb->slot_id);
			ret = -EINVAL;
		}
		if (!sdb->carrier_name || !sdb->carrier_data || \
		    !sdb->device_id) {
			dev_err(sdb->hwdev,
				"deivce nr %i: carrier name, "
				"data or dev_id not set\n", i);
			ret = -EINVAL;
		}
		if (ret)
			break;

	}
	if (ret) {
		kfree(devarray);
		return ret;
	}

	/* Validation is ok. Now init and register the devices */
	for (i = 0; i < n; i++) {
		sdb = devarray[i];

		sdb->nr_slots = n; /* each slot must know how many are there */
		sdb->devarray = devarray;

		device_initialize(&sdb->dev);
		sdb->dev.release = sdb_release;
		sdb->dev.parent = sdb->hwdev;

		/* Fill the identification stuff (may fail) */
		sdb_fill_id_info(sdb);

		sdb->dev.bus = &sdb_bus_type;

		/* Name from mezzanine info or carrier info. Or 0,1,2.. */
		device_id = sdb->device_id;
		if (!sdb->mezzanine_name)
			dev_set_name(&sdb->dev, "sdb-%04x", device_id);
		else
			dev_set_name(&sdb->dev, "%s-%04x", sdb->mezzanine_name,
				     device_id);
		ret = device_add(&sdb->dev);
		if (ret < 0) {
			dev_err(sdb->hwdev, "Slot %i: Failed in registering "
				"\"%s\"\n", sdb->slot_id, sdb->dev.kobj.name);
			goto out;
		}
		ret = sysfs_create_bin_file(&sdb->dev.kobj, &sdb_eeprom_attr);
		if (ret < 0) {
			dev_err(&sdb->dev, "Failed in registering eeprom\n");
			goto out1;
		}
		/* This device went well, give information to the user */
		sdb_dump_eeprom(sdb);
		sdb_dump_sdb(sdb);
	}
	return 0;

out1:
	device_del(&sdb->dev);
out:
	sdb_free_id_info(sdb);
	put_device(&sdb->dev);

	kfree(devarray);
	for (i--; i >= 0; i--) {
		sysfs_remove_bin_file(&devs[i]->dev.kobj, &sdb_eeprom_attr);
		device_del(&devs[i]->dev);
		sdb_free_id_info(devs[i]);
		put_device(&devs[i]->dev);
	}
	return ret;

}
EXPORT_SYMBOL(sdb_device_register_n);

int sdb_device_register(struct sdb_device *sdb)
{
	return sdb_device_register_n(&sdb, 1);
}
EXPORT_SYMBOL(sdb_device_register);

void sdb_device_unregister_n(struct sdb_device **devs, int n)
{
	int i;

	if (n < 1)
		return;

	/* Free devarray first, not used by the later loop */
	kfree(devs[0]->devarray);

	for (i = 0; i < n; i++) {
		sysfs_remove_bin_file(&devs[i]->dev.kobj, &sdb_eeprom_attr);
		device_del(&devs[i]->dev);
		sdb_free_id_info(devs[i]);
		put_device(&devs[i]->dev);
	}
}
EXPORT_SYMBOL(sdb_device_unregister_n);

void sdb_device_unregister(struct sdb_device *sdb)
{
	sdb_device_unregister_n(&sdb, 1);
}
EXPORT_SYMBOL(sdb_device_unregister);

/* Init and exit are trivial */
static int sdb_init(void)
{
	return bus_register(&sdb_bus_type);
}

static void sdb_exit(void)
{
	bus_unregister(&sdb_bus_type);
}

module_init(sdb_init);
module_exit(sdb_exit);

MODULE_VERSION(GIT_VERSION);
MODULE_LICENSE("GPL");
