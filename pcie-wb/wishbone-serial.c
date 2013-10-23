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

#if   LINUX_VERSION_CODE == KERNEL_VERSION(2,6,26)
#define API 1
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,39)
#define API 2 /* v2.6.32 v2.6.33 v2.6.34 v2.6.35 v2.6.36 v2.6.37 v2.6.38 v2.6.39 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
#define API 3 /* v3.0 v3.1 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
#define API 4 /* v3.2 v3.3 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
#define API 5 /* v3.4 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
#define API 6 /* v3.5 v3.6 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0) && LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define API 7 /* v3.7 v3.8 v3.9 */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#define API 8 /* v3.10+ */
#else
#error Unsupported kernel version
#define API 8
#endif

#if API <= 7

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

#if API <= 1
static void wishbone_serial_set_termios(struct usb_serial_port *port, struct ktermios *old_termios)
#else /* API >= 2 */
static void wishbone_serial_init_termios(struct tty_struct *tty)
#endif
{
#if API <= 1
	struct ktermios *termios = port->tty->termios;
#elif API <= 6
	struct ktermios *termios = tty->termios;
#else /* API >= 7 */
	struct ktermios *termios = &tty->termios;
#endif

	/*
	 * The empeg-car player wants these particular tty settings.
	 * You could, for example, change the baud rate, however the
	 * player only supports 115200 (currently), so there is really
	 * no point in support for changes to the tty settings.
	 * (at least for now)
	 *
	 * The default requirements for this device are:
	 */
	termios->c_iflag
		&= ~(IGNBRK	/* disable ignore break */
		| BRKINT	/* disable break causes interrupt */
		| PARMRK	/* disable mark parity errors */
		| ISTRIP	/* disable clear high bit of input characters */
		| INLCR		/* disable translate NL to CR */
		| IGNCR		/* disable ignore CR */
		| ICRNL		/* disable translate CR to NL */
		| IXON);	/* disable enable XON/XOFF flow control */

	termios->c_oflag
		&= ~OPOST;	/* disable postprocess output characters */

	termios->c_lflag
		&= ~(ECHO	/* disable echo input characters */
		| ECHONL	/* disable echo new line */
		| ICANON	/* disable erase, kill, werase, and rprnt special characters */
		| ISIG		/* disable interrupt, quit, and suspend special characters */
		| IEXTEN);	/* disable non-POSIX special characters */

	termios->c_cflag
		&= ~(CSIZE	/* no size */
		| PARENB	/* disable parity bit */
		| CBAUD);	/* clear current baud rate */

	termios->c_cflag
		|= CS8;		/* character size 8 bits */

#if API <= 1
	port->tty->low_latency = 1;
	tty_encode_baud_rate(port->tty, 115200, 115200);
#else /* API >= 2 */
	tty_encode_baud_rate(tty, 115200, 115200);
#endif
}

#if API <= 1
static int wishbone_serial_open(struct usb_serial_port *port, struct file *filp)
#else /* API >= 2 */
static int wishbone_serial_open(struct tty_struct *tty, struct usb_serial_port *port)
#endif
{
	int retval;

	retval = usb_gsi_openclose(port, 1);
	if (retval) {
		dev_err(&port->serial->dev->dev,
		       "Could not mark device as open (%d)\n",
		       retval);
		return retval;
	}
	
#if API <= 1
	wishbone_serial_set_termios(port, NULL);
#endif

#if API <= 1
	retval = usb_serial_generic_open(port, filp);
#else /* API >= 2 */
	retval = usb_serial_generic_open(tty, port);
#endif
	if (retval)
		usb_gsi_openclose(port, 0);

	return retval;
}

#if API <= 1
static void wishbone_serial_close(struct usb_serial_port *port, struct file *filp)
#else /* API >= 2 */
static void wishbone_serial_close(struct usb_serial_port *port)
#endif
{
#if API <= 2
	usb_kill_urb(port->write_urb);
	usb_kill_urb(port->read_urb);
#else /* API >= 3 */
	usb_serial_generic_close(port);
#endif
	usb_gsi_openclose(port, 0);
}

#if API <= 5
static struct usb_driver wishbone_serial_driver = {
	.name =		"wishbone_serial",
	.probe =	usb_serial_probe,
	.disconnect =	usb_serial_disconnect,
	.id_table =	id_table,
#if API <= 4
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
#if API <= 5
	.usb_driver =		&wishbone_serial_driver,
#endif
	.num_ports =		1,
	.open =			wishbone_serial_open,
	.close =		wishbone_serial_close,
#if API <= 1
	.set_termios =		wishbone_serial_set_termios,
#else /* API >= 2 */
	.init_termios =		wishbone_serial_init_termios,
#endif
};

#if API <= 4

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

#else /* API >= 5 */

static struct usb_serial_driver * const serial_drivers[] = {
	&wishbone_serial_device, NULL
};

#if API <= 5
module_usb_serial_driver(wishbone_serial_driver, serial_drivers);
#else /* API >= 6 */
module_usb_serial_driver(serial_drivers, id_table);
#endif

#endif

MODULE_AUTHOR("Wesley W. Terpstra <w.terpstra@gsi.de>");
MODULE_DESCRIPTION("USB Wishbone-Serial adapter");
MODULE_LICENSE("GPL");

#else /* API >= 8 */
/* Supported in mainline */
#endif
