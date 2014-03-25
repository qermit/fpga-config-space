/*
* Copyright (C) 2012-2013 GSI (www.gsi.de)
* Author: Cesar Prados <c.prados@gsi.de>
*
* Released according to the GNU GPL, version 2 or any later version
*
* VME-WB bridge for VME
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/io.h>
#include "vme_wb.h"
#include "wishbone.h"

#if defined(__BIG_ENDIAN)
#define endian_addr(width, shift) (sizeof(wb_data_t)-width)-shift
#elif defined(__LITTLE_ENDIAN)
#define endian_addr(width, shift) shift
#else
#error "unknown machine byte order (endian)"
#endif

/* Module parameters */
static int slot[VME_MAX_DEVICES];
static unsigned int slot_num;
static unsigned int vmebase[VME_MAX_DEVICES];
static unsigned int vmebase_num;
static int vector[VME_MAX_DEVICES];
static int level[VME_MAX_DEVICES];
static unsigned int vector_num;
static unsigned int vector_lev;
static int lun[VME_MAX_DEVICES] = VME_DEFAULT_IDX;
static unsigned int lun_num;
static unsigned int debug = 0;

static void wb_cycle(struct wishbone *wb, int on)
{
}

static wb_data_t wb_read_cfg(struct wishbone *wb, wb_addr_t addr)
{
	return 0;
}

static void wb_write(struct wishbone *wb, wb_addr_t addr, wb_data_t data)
{
}

static wb_data_t wb_read(struct wishbone *wb, wb_addr_t addr)
{
   return 0;
}

static int wb_request(struct wishbone *wb, struct wishbone_request *req)
{
	return 0;
}

static void wb_reply(struct wishbone *wb, int err, wb_data_t data)
{
}
static void wb_byteenable(struct wishbone *wb, unsigned char be)
{
}

static const struct wishbone_operations wb_ops = {
	.cycle = wb_cycle,
	.byteenable = wb_byteenable,
	.write = wb_write,
	.read = wb_read,
	.read_cfg = wb_read_cfg,
	.request = wb_request,
	.reply = wb_reply,
};

static int __init vme_init(void)
{
	printk(KERN_ALERT "This module is under development, you should load it\n");
	return 0;
}

static void __exit vme_exit(void)
{
		printk(KERN_ALERT "Removing the module \n");
}

module_init(vme_init);
module_exit(vme_exit);

module_param_array(slot, int, &slot_num, S_IRUGO);
MODULE_PARM_DESC(slot, "Slot where VME card is installed");
module_param_array(vmebase, uint, &vmebase_num, S_IRUGO);
MODULE_PARM_DESC(vmebase, "VME Base address of the VME card registers");

module_param_array(vector, int, &vector_num, S_IRUGO);
MODULE_PARM_DESC(vector, "IRQ vector");

module_param_array(level, int, &vector_lev, S_IRUGO);
MODULE_PARM_DESC(level, "IRQ level");
module_param_array(lun, int, &lun_num, S_IRUGO);
MODULE_PARM_DESC(lun, "Index value for VME card");
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debugging information");

MODULE_AUTHOR("Cesar Prados Boda");
MODULE_LICENSE("GPL");
/* MODULE_VERSION(GIT_VERSION); */
MODULE_VERSION("v0.1");
MODULE_DESCRIPTION("vme wb bridge driver");
