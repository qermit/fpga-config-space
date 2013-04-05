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

#define GSI_VENDOR_OPENCLOSE 0xB0

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

static int usb_wb_open(struct tty_struct *tty, struct usb_serial_port *port)
{
	struct usb_device *dev = port->serial->dev;
	int result;
	
	result = usb_control_msg(
		dev, 
		usb_sndctrlpipe(dev, 0), /* Send to EP0OUT */
		GSI_VENDOR_OPENCLOSE,
		USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
		1,     /* wValue (2..3) = device is open */
		port->serial->interface->cur_altsetting->desc.bInterfaceNumber,
		0, 0,  /* no data stage */
		5000); /* timeout */
	
	if (result < 0)
		dev_err(&dev->dev, "Could not mark device as open (result = %d)\n", result);
	
	return usb_serial_generic_open(tty, port);
}

static void usb_wb_close(struct usb_serial_port *port)
{
	struct usb_device *dev = port->serial->dev;
	int result;
	
	result = usb_control_msg(
		dev, 
		usb_sndctrlpipe(dev, 0), /* Send to EP0OUT */
		GSI_VENDOR_OPENCLOSE,
		USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_INTERFACE,
		0,     /* wValue (2..3) = device is closed */
		port->serial->interface->cur_altsetting->desc.bInterfaceNumber,
		0, 0,  /* no data stage */
		5000); /* timeout */
	
	if (result < 0)
		dev_err(&dev->dev, "Could not mark device as closed (result = %d)\n", result);
	
	return usb_serial_generic_close(port);
}                

static struct usb_serial_driver usb_wb_device = {
	.driver = {
		.owner =	THIS_MODULE,
		.name =		"usb_wb",
	},
	.id_table =		id_table,
	.usb_driver =		&usb_wb_driver,
	.num_ports =		1,
	.open =			&usb_wb_open,
	.close =		&usb_wb_close,
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
