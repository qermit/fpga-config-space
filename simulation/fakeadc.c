/*
 * Fake DAC Wishbone driver
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

#define WB_ADC_VENDOR 	0x0
#define WB_ADC_DEVICE	0x1

static struct wb_device_id fakeadc_ids[] = {
	{ WB_ADC_VENDOR, WB_ADC_DEVICE, WB_NO_CLASS},
	{ 0, },
};

static int fakeadc_probe(struct wb_device *dev)
{
	printk(KERN_INFO KBUILD_MODNAME ": %s\n", __func__);
	return 0;
}

static int fakeadc_remove(struct wb_device *dev)
{
	printk(KERN_INFO KBUILD_MODNAME ": %s\n", __func__);
	return 0;
}

static struct wb_driver fakeadc_driver = {
	.name = "fakeadc",
	.owner = THIS_MODULE,
	.id_table = fakeadc_ids,
	.probe = fakeadc_probe,
	.remove = fakeadc_remove,
};

static int fakeadc_init(void)
{
	return wb_register_driver(&fakeadc_driver);
}

static void fakeadc_exit(void)
{
	wb_unregister_driver(&fakeadc_driver);
}

module_init(fakeadc_init);
module_exit(fakeadc_exit);

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("Fake ADC wishbone driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
