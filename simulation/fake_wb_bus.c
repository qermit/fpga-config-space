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

#include <linux/version.h>
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

const struct firmware *wb_fw;

LIST_HEAD(spec_devices);
static struct mutex list_lock;

void *fake_read_cfg(struct wb_bus *bus, wb_addr_t addr, void *buf, size_t len)
{
	if (wb_fw->size < addr + len)
		return NULL;
	memcpy(buf, &wb_fw->data[addr], len);
	return buf;
}

struct wb_ops fake_wb_ops = {
	.read_cfg = fake_read_cfg,
};

struct wb_bus fake_wb_bus = {
	.name = "fake_wb_bus",
	.owner = THIS_MODULE,
	.ops = &fake_wb_ops,
	.sdwb_header_base = 0,
};

static int fake_wbbus_probe(struct device *dev)
{
	int ret;
	char fwname[64];

	/*
	 * load firmware with wishbone address map. In the real driver,
	 * we would first load the bitstream into the fpga and then read
	 * the header from its appropriate location.
	 *
	 * Below, we just use the PCI bus and slot number to get the firmware
	 * file.
	 */
	sprintf(fwname, "fakespec-%04x-%04x", spec_vendor, spec_device);
	if ((ret = request_firmware(&wb_fw, fwname, dev)) != 0) {
		pr_err(KBUILD_MODNAME ": failed to load "
		       "firmware \"%s\"\n", fwname);
		return ret;
	}

	if ((ret = wb_register_bus(&fake_wb_bus)) < 0) {
		goto bus_register_fail;
	}

	return 0;

bus_register_fail:
	release_firmware(wb_fw);
	return ret;
}

static void fake_wbbus_release(struct device *dev)
{
}

static struct device fake_wbbus_device = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	.bus_id = "fake-wbbus0",
#else
	.init_name = "fake-wbbus0",
#endif
	.release = fake_wbbus_release,
};

static int __init fake_wb_bus_init(void)
{
	int ret;

	mutex_init(&list_lock);

	ret = device_register(&fake_wbbus_device);
	if (ret) {
		pr_err(KBUILD_MODNAME ": failed to register fake"
					"Wishbone bus\n");
		return ret;
	}
	if ((ret = fake_wbbus_probe(&fake_wbbus_device)) < 0) {
		device_unregister(&fake_wbbus_device);
		return ret;
	}
	return 0;
}

static void __exit fake_wb_bus_exit(void)
{
	wb_unregister_bus(&fake_wb_bus);
	release_firmware(wb_fw);
	device_unregister(&fake_wbbus_device);
}

module_init(fake_wb_bus_init);
module_exit(fake_wb_bus_exit);

MODULE_AUTHOR("Manohar Vanga <mvanga@cern.ch>");
MODULE_DESCRIPTION("Fake Spec board driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
