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

#include "wb_onewire.h"

#define OPENCORES_ONEWIRE_VENDOR	0x0
#define OPENCORES_ONEWIRE_DEVICE	0x1

static LIST_HEAD(wb_onewire_list);
static DEFINE_MUTEX(onewire_list_lock);

struct wb_onewire_dev {
	int num;
	struct wb_device *wbdev;
	wb_addr_t base;
	struct mutex lock;
	struct list_head list;
};
#define to_wb_onewire_dev(dev) ((struct wb_onewire_dev *)wb_get_drvdata(dev))

void wb_onewire_devinit(struct wb_onewire_dev *dev)
{
	uint32_t data;
	int clk_div_nor = 624;
	int clk_div_ovd = 124;
	data = ((clk_div_nor&0xffff) | ((clk_div_ovd<<16) & (0xffff<<16)));
	wb_writel(dev->wbdev->bus, dev->base + 0x4, data);
}

uint32_t wb_onewire_slot(struct wb_onewire_dev *dev, int port, int bit)
{
	uint32_t data, reg;
	data = (((port<<8) & (0xf<<8)) | (1<<3) | (bit & (1<<0)));
	wb_writel(dev->wbdev->bus, dev->base + 0x0, data);
	while (wb_readl(dev->wbdev->bus, dev->base + 0x0) & (1 << 3));
	reg = wb_readl(dev->wbdev->bus, dev->base + 0x0);
	return (reg & (1 << 0));
}

uint32_t wb_onewire_writebit(struct wb_onewire_dev *dev, int port, int bit)
{
	return wb_onewire_slot(dev, port, bit);
}

uint32_t wb_onewire_readbit(struct wb_onewire_dev *dev, int port)
{
	return wb_onewire_slot(dev, port, 0x1);
}

uint8_t wb_onewire_readbyte(struct wb_onewire_dev *dev, int port)
{
	int i;
	uint8_t data = 0;
	for (i = 0; i < 8; i++)
		data |= wb_onewire_readbit(dev, port) << i;
	return data;
}

int wb_onewire_writebyte(struct wb_onewire_dev *dev, int port, uint8_t byte)
{
	int i;
	uint8_t data = 0;
	uint32_t tmp;
	uint8_t byte_old = byte;
	for (i = 0; i < 8; i++) {
		data |= ((tmp = wb_onewire_writebit(dev, port, byte & 0x1)) << i);
		byte >>= 1;
	}
	if (byte_old != data)
		return -1;
	return 0;
}

void wb_onewire_readblock(struct wb_onewire_dev *dev, int port, uint8_t *buf, int len)
{
	int i = 0;
	if (len > 160)
		return;
	for (i = 0; i < len; i++)
		*buf++ = wb_onewire_readbyte(dev, port);
}

void wb_onewire_writeblock(struct wb_onewire_dev *dev, int port, uint8_t *buf, int len)
{
	int i = 0;
	if (len > 160)
		return;
	for (i = 0; i < len; i++)
		wb_onewire_writebyte(dev, port, buf[i]);
}

uint32_t wb_onewire_reset(struct wb_onewire_dev *dev, int port)
{
	uint32_t data, reg;
	data = (((port<<8) & (0xf<<8)) | (1<<3) | (1<<1));
	wb_writel(dev->wbdev->bus, dev->base + 0x0, data);
	while (wb_readl(dev->wbdev->bus, dev->base + 0x0) & (1<<3));
	reg = wb_readl(dev->wbdev->bus, dev->base + 0x0);
	return ((~reg) & (1<<0));
}

void ds18b20_read_serial(struct wb_onewire_dev *dev, int port, uint8_t *sbuf)
{
	int i;

	wb_onewire_reset(dev, port);
	wb_onewire_writebyte(dev, port, 0x33);
	sbuf[0] = wb_onewire_readbyte(dev, port);
	for (i = 6; i >= 1; i--)
		sbuf[i] = wb_onewire_readbyte(dev, port);
	sbuf[7] = wb_onewire_readbyte(dev, port);
}

void ds18b20_access(struct wb_onewire_dev *dev, int port, uint8_t *serial)
{
	int i;

	wb_onewire_reset(dev, port);
	wb_onewire_writebyte(dev, port, 0x55);
	for (i = 0; i < 8; i++)
		serial[i] = serial[i] & 0xff;
	wb_onewire_writeblock(dev, port, serial, 8);
}

int ds18b20_read_temp(struct wb_onewire_dev *dev, int port, uint8_t *serial)
{
	int i;
	uint8_t data[9];
	int temp;
	ds18b20_access(dev, port, serial);
	printk("serial: ");
	for (i = 0; i < 8; i++)
		printk("%02x ", serial[i]);
	printk("\n");
	wb_onewire_writebyte(dev, port, 0x44);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(HZ);
	ds18b20_access(dev, port, serial);
	wb_onewire_writebyte(dev, port, 0xbe);
	wb_onewire_readblock(dev, port, data, 9);
	printk("data: ");
	for (i = 0; i < 9; i++)
		printk("%2x ", data[i]);
	printk("\n");
	temp = (data[1] << 8) | data[0];
	if (temp & 0x1000)
		temp = -0x10000 + temp;
	return temp;
        //temp = temp/16.0
}

int read_onewire_temp(struct wb_onewire_dev *dev)
{
	int temp = 0;
	uint8_t serial[10];
	ds18b20_read_serial(dev, 0, serial);
	temp = ds18b20_read_temp(dev, 0, serial);
	return temp;
}

static int wb_onewire_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int wb_onewire_release(struct inode *inode, struct file *file)
{
	return 0;
}

int wb_onewire_ioctl(struct inode *inode, struct file *file,
	unsigned int ioctl_num,	unsigned long ioctl_param)
{
	struct wb_onewire_dev *dev;
	struct wb_onewire_dev *found = NULL;
	struct wb_onewire_arg *arg = (struct wb_onewire_arg *)ioctl_param;
	switch (ioctl_num) {
	case READ_TEMP: {
		printk("looking for device %d\n", arg->num);
		mutex_lock(&onewire_list_lock);
		list_for_each_entry(dev, &wb_onewire_list, list) {
			if (dev->num == arg->num) {
				found = dev;
				break;
			}
		}
		mutex_unlock(&onewire_list_lock);
		if (!found)
			return -EFAULT;
		arg->temp= read_onewire_temp(found);
		printk("Got temp: %08x\n", arg->temp);
		break;
	}
	}
	return 0;
}

struct file_operations wb_onewire_ops = {
	.open = wb_onewire_open,
	.release = wb_onewire_release,
	.ioctl = wb_onewire_ioctl,
};

struct miscdevice misc_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "onewire",
	.fops = &wb_onewire_ops,
};


static struct wb_device_id wb_onewire_ids[] = {
	{ OPENCORES_ONEWIRE_VENDOR, OPENCORES_ONEWIRE_DEVICE, WB_NO_CLASS },
	{ 0, },
};

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

	wb_onewire_devinit(owdev);

	mutex_lock(&onewire_list_lock);
	list_add(&owdev->list, &wb_onewire_list);
	mutex_unlock(&onewire_list_lock);

	printk("registered device %d\n", owdev->num);

	return 0;
}

static int wb_onewire_remove(struct wb_device *dev)
{
	struct wb_onewire_dev *owdev = to_wb_onewire_dev(dev);

	pr_info(KBUILD_MODNAME ": %s\n", __func__);

	mutex_lock(&onewire_list_lock);
	list_del(&owdev->list);
	mutex_unlock(&onewire_list_lock);

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
	if ((ret = misc_register(&misc_dev)))
		return	ret;

	if ((ret = wb_register_driver(&wb_onewire_driver))) {
		misc_deregister(&misc_dev);
		return ret;
	}
	return 0;
}

static void wb_onewire_exit(void)
{
	wb_unregister_driver(&wb_onewire_driver);
	misc_deregister(&misc_dev);
}

module_init(wb_onewire_init);
module_exit(wb_onewire_exit);

MODULE_AUTHOR("Manohar Vanga");
MODULE_DESCRIPTION("OpenCores 1-wire Wishbone Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
