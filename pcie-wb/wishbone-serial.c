/*
 * USB Wishbone-Serial adapter driver
 *
 * Copyright (C) 2013 Wesley W. Terpstra <w.terpstra@gsi.de>
 * Copyright (C) 2013 GSI Helmholtz Centre for Heavy Ion Research GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tty.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/serial.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#define GSI_VENDOR_OPENCLOSE 0xB0

/* v1 2a1e7d5d 2010-03-10 2.6.34-26 oldest kernel supported by this driver
 * v2 dba607f9 2012-02-28 3.3-rc4   remove .no_dynamic_id, .usb_driver, _init and _exit
 * v3 68e24113 2012-05-09 3.4-rc6   changes module_usb_serial_driver
 * ... anything newer doesn't matter as driver is now in mainline.
 */

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,3,0)
#define API 1
#else
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,4,0)
#define API 2
#else
#define API 3
#endif
#endif

static const struct usb_device_id id_table[] = {
	{ USB_DEVICE_AND_INTERFACE_INFO(0x1D50, 0x6062, 0xFF, 0xFF, 0xFF) },
	{ },
};
MODULE_DEVICE_TABLE(usb, id_table);

/*
 * Etherbone must be told that a new stream has begun before data arrives.
 * This is necessary to restart the negotiation of Wishbone bus parameters.
 * Similarly, when the stream ends, Etherbone must be told so that the cycle
 * line can be driven low in the case that userspace failed to do so.
 */
static int usb_gsi_openclose(struct usb_serial_port *port, int value)
{
	struct usb_device *dev = port->serial->dev;

	return usb_control_msg(
		dev,
		usb_sndctrlpipe(dev, 0), /* Send to EP0OUT */
		GSI_VENDOR_OPENCLOSE,
		USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
		value, /* wValue = device is open(1) or closed(0) */
		port->serial->interface->cur_altsetting->desc.bInterfaceNumber,
		NULL, 0,  /* There is no data stage */
		5000); /* Timeout till operation fails */
}

static int wishbone_serial_open(struct tty_struct *tty,
				struct usb_serial_port *port)
{
	int retval;

	retval = usb_gsi_openclose(port, 1);
	if (retval) {
		dev_err(&port->serial->dev->dev,
		       "Could not mark device as open (%d)\n",
		       retval);
		return retval;
	}

	retval = usb_serial_generic_open(tty, port);
	if (retval)
		usb_gsi_openclose(port, 0);

	return retval;
}

static void wishbone_serial_close(struct usb_serial_port *port)
{
	usb_serial_generic_close(port);
	usb_gsi_openclose(port, 0);
}

#if API < 3
static struct usb_driver wishbone_serial_driver = {
	.name =		"wishbone_serial",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table,
#if API == 1
	.no_dynamic_id =	1,
#endif
};
#endif

static struct usb_serial_driver wishbone_serial_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"wishbone_serial",
	},
	.id_table =		id_table,
#if API == 1
	.usb_driver =		&wishbone_serial_driver,
#endif
	.num_ports =		1,
	.open =			&wishbone_serial_open,
	.close =		&wishbone_serial_close,
};

#if API == 1

static int __init wishbone_serial_init(void)
{
	int retval;

	retval = usb_serial_register(&wishbone_serial_device);
	if (retval)
		return retval;
	retval = usb_register(&wishbone_serial_driver);
	if (retval)
		usb_serial_deregister(&wishbone_serial_device);
	return retval;
}

static void __exit wishbone_serial_exit(void)
{
	usb_deregister(&wishbone_serial_driver);
	usb_serial_deregister(&wishbone_serial_device);
}

module_init(wishbone_serial_init);
module_exit(wishbone_serial_exit);

#else

static struct usb_serial_driver * const serial_drivers[] = {
	&wishbone_serial_device, NULL
};

#if API == 2
module_usb_serial_driver(wishbone_serial_driver, serial_drivers);
#else
module_usb_serial_driver(serial_drivers, id_table);
#endif

#endif

MODULE_AUTHOR("Wesley W. Terpstra <w.terpstra@gsi.de>");
MODULE_DESCRIPTION("USB Wishbone-Serial adapter");
MODULE_LICENSE("GPL");

