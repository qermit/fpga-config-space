/*
 * USB-WB adapter driver
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License version
 *	2 as published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/uaccess.h>

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1D50, 0x6062, 0xFF, 0xFF, 0xFF) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver usb_wb_driver = {
	.name =		"usb_wb",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table,
	.no_dynamic_id =	1,
};

static struct usb_serial_driver usb_wb_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"usb_wb",
	},
	.id_table =		id_table,
	.usb_driver =		&usb_wb_driver,
	.num_ports =		1,
};

static int __init usb_wb_init(void)
{
	int retval;

	retval = usb_serial_register(&usb_wb_device);
	if (retval)
		return retval;
	retval = usb_register(&usb_wb_driver);
	if (retval)
		usb_serial_deregister(&usb_wb_device);
	return retval;
}

static void __exit usb_wb_exit(void)
{
	usb_deregister(&usb_wb_driver);
	usb_serial_deregister(&usb_wb_device);
}

MODULE_AUTHOR("Wesley W. Terpstra <w.terpstra@gsi.de>");
MODULE_DESCRIPTION("Wishbone-USB adapter");
MODULE_LICENSE("GPL");

module_init(usb_wb_init);
module_exit(usb_wb_exit);
