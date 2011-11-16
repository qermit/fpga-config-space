/*
 * Fake ADC Wishbone driver
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

#define WB_DAC_VENDOR	0x0
#define WB_DAC_DEVICE	0x2

static struct wb_device_id fakedac_ids[] = {
	{ WB_DAC_VENDOR, WB_DAC_DEVICE, WB_NO_CLASS},
	{ 0, },
};

static int fakedac_probe(struct wb_device *dev)
{
	pr_info(KBUILD_MODNAME ": %s\n", __func__);
	return 0;
}

static int fakedac_remove(struct wb_device *dev)
{
	pr_info(KBUILD_MODNAME ": %s\n", __func__);
	return 0;
}

static struct wb_driver fakedac_driver = {
	.name = "fakedac",
	.owner = THIS_MODULE,
	.id_table = fakedac_ids,
	.probe = fakedac_probe,
	.remove = fakedac_remove,
};

static int fakedac_init(void)
{
	return wb_register_driver(&fakedac_driver);
}

static void fakedac_exit(void)
{
	wb_unregister_driver(&fakedac_driver);
}

module_init(fakedac_init);
module_exit(fakedac_exit);

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("Fake DAC wishbone driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
