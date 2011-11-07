/*
 * Fake Wishbone bus driver
 *
 * Author: Manohar Vanga <manohar.vanga@cern.ch>
 * Copyright (C) 2011 CERN
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/mutex.h>

#include <linux/wishbone.h>
#include <linux/sdwb.h>

int spec_vendor = 0xbabe;
int spec_device = 0xbabe;
module_param(spec_vendor, int, S_IRUGO);
module_param(spec_device, int, S_IRUGO);

static int ndev;

LIST_HEAD(spec_devices);
static struct mutex list_lock;

static int fake_wbbus_probe(struct device *dev)
{
	char fwname[64];
	unsigned int header_addr = 0; /* Normally read from the BAR */
	struct sdwb_head *header;
	struct sdwb_wbid *id;
	struct sdwb_wbd *wbd;
	struct wb_device *wbdev, *next;
	const struct firmware *wb_fw;

	/*
	 * load firmware with wishbone address map. In the real driver,
	 * we would first load the bitstream into the fpga and then read
	 * the header from its appropriate location.
	 *
	 * For loading the bitstream, we read the bitstream ID off
	 * an eeprom on the board or some similar way.
	 *
	 * Below, we just use the PCI bus and slot number to get the firmware
	 * file.
	 */
	sprintf(fwname, "fakespec-%04x-%04x", spec_vendor, spec_device);
	if (request_firmware(&wb_fw, fwname, dev)) {
		pr_err(KBUILD_MODNAME ": failed to load "
		       "firmware \"%s\"\n", fwname);
		return -1;
	}

	header = (struct sdwb_head *)&wb_fw->data[header_addr];
	if (header->magic != SDWB_HEAD_MAGIC) {
		pr_err(KBUILD_MODNAME ": invalid sdwb header at %p "
		       "(magic %llx)\n", header, header->magic);
		goto head_fail;
	}

	id = (struct sdwb_wbid *)&wb_fw->data[header->wbid_address];
	pr_info(KBUILD_MODNAME ": found sdwb bistream: 0x%llx\n",
	       id->bstream_type);

	wbd = (struct sdwb_wbd *)&wb_fw->data[header->wbd_address];
	while (wbd->wbd_magic == SDWB_WBD_MAGIC) {
		wbdev = kzalloc(sizeof(struct wb_device), GFP_KERNEL);
		if (!wbdev)
			goto register_fail;
		memcpy(&wbdev->wbd, wbd, sizeof(struct sdwb_wbd));
		if (wb_register_device(wbdev) < 0)
			goto register_fail;
		mutex_lock(&list_lock);
		list_add(&wbdev->list, &spec_devices);
		ndev++;
		mutex_unlock(&list_lock);
		wbd++;
	}
	release_firmware(wb_fw);
	pr_info(KBUILD_MODNAME ": found %d wishbone devices\n", ndev);
	return 0;

register_fail:
	kfree(wbdev);
	mutex_lock(&list_lock);
	list_for_each_entry_safe(wbdev, next, &spec_devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
		kfree(wbdev);
	}
	mutex_unlock(&list_lock);

head_fail:
	release_firmware(wb_fw);
	return -1;
}

static void fake_wbbus_release(struct device *dev)
{
	struct wb_device *wbdev, *next;

	mutex_lock(&list_lock);
	list_for_each_entry_safe(wbdev, next, &spec_devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
		kfree(wbdev);
	}
	mutex_unlock(&list_lock);
}

static struct device fake_wbbus_device = {
	.init_name = "fake-wbbus0",
	.release = fake_wbbus_release,
};

static int __init fake_wb_bus_init(void)
{
	int ret;

	mutex_init(&list_lock);

	ret = device_register(&fake_wbbus_device);
	if (ret) {
		pr_err(KBUILD_MODNAME "failed to register fake"
					"Wishbone bus\n");
		return ret;
	}
	fake_wbbus_probe(&fake_wbbus_device);
	return 0;
}

static void __exit fake_wb_bus_exit(void)
{
	device_unregister(&fake_wbbus_device);
}

module_init(fake_wb_bus_init);
module_exit(fake_wb_bus_exit);

MODULE_AUTHOR("Manohar Vanga <mvanga@cern.ch>");
MODULE_DESCRIPTION("Fake Spec board driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
