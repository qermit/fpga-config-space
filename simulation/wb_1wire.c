/*
 * OpenCores 1-wire Wishbone Driver
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

#include <linux/wishbone.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <w1.h>
#include <w1_int.h>

#include "wb_onewire.h"

#define OPENCORES_ONEWIRE_VENDOR	0x0
#define OPENCORES_ONEWIRE_DEVICE	0x1

#define OCOW_REG_CSR			0x0
#define OCOW_REG_CDR			0x4

#define OCOW_CSR_DAT_MASK		(1<<0)
#define OCOW_CSR_RST_MASK		(1<<1)
#define OCOW_CSR_OVD_MASK		(1<<2)
#define OCOW_CSR_CYC_MASK		(1<<3)
#define OCOW_CSR_PWR_MASK		(1<<4)
#define OCOW_CSR_IRQ_MASK		(1<<6)
#define OCOW_CSR_IEN_MASK		(1<<7)
#define OCOW_CSR_SEL_OFS		(8)
#define OCOW_CSR_SEL_MASK		(0xF<<8)
#define OCOW_CSR_POWER_OFS		(16)
#define OCOW_CSR_POWER_MASK		(0xFFFF<<16)

#define OCOW_CDR_NOR_MASK		(0xFFFF<<0)
#define OCOW_CDR_OVD_OFS		(16)
#define OCOW_CDR_OVD_MASK		(0XFFFF<<16)


struct wb_onewire_dev {
	int num;
	int port;
	int clk_div_nor;
	int clk_div_ovd;
	struct wb_device *wbdev;
	wb_addr_t base;
	struct w1_bus_master master;
	struct mutex lock;
};
#define to_wb_onewire_dev(dev) ((struct wb_onewire_dev *)wb_get_drvdata(dev))

void wb_onewire_init_lowlevel(struct wb_onewire_dev *dev)
{
	uint32_t data;

	data = ((dev->clk_div_nor & OCOW_CDR_NOR_MASK) |
		((dev->clk_div_ovd << OCOW_CDR_OVD_OFS) & OCOW_CDR_OVD_MASK));
	wb_writel(dev->wbdev->bus, dev->base + OCOW_REG_CDR, data);
}

static u8 wb_onewire_touch_bit(void *data, u8 bit)
{
	uint32_t val;
	uint32_t reg;
	struct wb_onewire_dev *dev = (struct wb_onewire_dev *)data;

	val = (((dev->port << OCOW_CSR_SEL_OFS) & OCOW_CSR_SEL_MASK) |
		OCOW_CSR_CYC_MASK |
		(bit & OCOW_CSR_DAT_MASK));

	wb_writel(dev->wbdev->bus, dev->base + OCOW_REG_CSR, val);

	/* Wait for the CSR_CYC bit to be unset */
	while (wb_readl(dev->wbdev->bus, dev->base + OCOW_REG_CSR) &
		OCOW_CSR_CYC_MASK);

	reg = wb_readl(dev->wbdev->bus, dev->base + OCOW_REG_CSR);

	return (reg & OCOW_CSR_DAT_MASK);
}

static u8 wb_onewire_reset(void *data)
{
	uint32_t val;
	uint32_t reg;
	struct wb_onewire_dev *dev = (struct wb_onewire_dev *)data;

	val = (((dev->port << OCOW_CSR_SEL_OFS) & OCOW_CSR_SEL_MASK) |
		OCOW_CSR_CYC_MASK |
		OCOW_CSR_RST_MASK);

	wb_writel(dev->wbdev->bus, dev->base + OCOW_REG_CSR, val);

	/* Wait for the CSR_CYC bit to be unset */
	while (wb_readl(dev->wbdev->bus, dev->base + OCOW_REG_CSR) &
		OCOW_CSR_CYC_MASK);

	reg = wb_readl(dev->wbdev->bus, dev->base + OCOW_REG_CSR);

	return (reg & OCOW_CSR_DAT_MASK);
}

static struct wb_device_id wb_onewire_ids[] = {
	{ OPENCORES_ONEWIRE_VENDOR, OPENCORES_ONEWIRE_DEVICE, WB_NO_CLASS },
	{ 0, },
};

int wb_onewire_dev_init(struct wb_onewire_dev *dev)
{
	int ret;

	memset(&dev->master, 0, sizeof(struct w1_bus_master));

	dev->master.data = dev;
	dev->master.touch_bit = &wb_onewire_touch_bit;
	dev->master.reset_bus = &wb_onewire_reset;

	ret = w1_add_master_device(&dev->master);
	if (ret < 0)
		return ret;

	wb_onewire_init_lowlevel(dev);

	return 0;
}

static int wb_onewire_probe(struct wb_device *dev)
{
	struct wb_onewire_dev *owdev;
	static atomic_t devno = ATOMIC_INIT(-1);

	pr_info(KBUILD_MODNAME ": %s\n", __func__);

	owdev = kzalloc(sizeof(struct wb_onewire_dev), GFP_KERNEL);
	if (!owdev) {
		pr_err(KBUILD_MODNAME ": failed to allocate device\n");
		return -ENOMEM;
	}
	owdev->wbdev = dev;
	owdev->base = dev->wbd.hdl_base;
	wb_set_drvdata(dev, owdev);
	mutex_init(&owdev->lock);
	owdev->num = atomic_inc_return(&devno);
	owdev->port = 0;
	owdev->clk_div_nor = 624;
	owdev->clk_div_ovd = 124;

	wb_onewire_dev_init(owdev);

	return 0;
}

static int wb_onewire_remove(struct wb_device *dev)
{
	struct wb_onewire_dev *owdev = to_wb_onewire_dev(dev);

	pr_info(KBUILD_MODNAME ": %s\n", __func__);

	w1_remove_master_device(&owdev->master);
	kfree(owdev);

	return 0;
}

static struct wb_driver wb_onewire_driver = {
	.name = "wb_onewire",
	.owner = THIS_MODULE,
	.id_table = wb_onewire_ids,
	.probe = wb_onewire_probe,
	.remove = wb_onewire_remove,
};

static int wb_onewire_init(void)
{
	int ret;

	if ((ret = wb_register_driver(&wb_onewire_driver)) < 0)
		return ret;
	return 0;
}

static void wb_onewire_exit(void)
{
	wb_unregister_driver(&wb_onewire_driver);
}

module_init(wb_onewire_init);
module_exit(wb_onewire_exit);

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("OpenCores 1-wire Wishbone Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
