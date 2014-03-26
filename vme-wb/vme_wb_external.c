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
#include <vmebus.h>
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
	unsigned char *ctrl_win;
	struct vme_wb_dev *dev;

	dev = container_of(wb, struct vme_wb_dev, wb);

	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

	if (on)
		mutex_lock(&dev->mutex);

	if (unlikely(debug))
		printk(KERN_ALERT ": Cycle(%d)\n", on);

	iowrite32(cpu_to_be32((on ? 0x80000000UL : 0) + 0x40000000UL),
		  ctrl_win + CTRL);

	if (!on)
		mutex_unlock(&dev->mutex);
}

static wb_data_t wb_read_cfg(struct wishbone *wb, wb_addr_t addr)
{
	wb_data_t out;
	struct vme_wb_dev *dev;
	unsigned char *ctrl_win;

	dev = container_of(wb, struct vme_wb_dev, wb);
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

	if (unlikely(debug))
		printk(KERN_ALERT VME_WB ": READ CFG  addr %d \n", addr);

	switch (addr) {
	case 0:
		out = 0;
		break;
	case 4:
		out = be32_to_cpu(ioread32(ctrl_win + ERROR_FLAG));
		break;
	case 8:
		out = 0;
		break;
	case 12:
		out = be32_to_cpu(ioread32(ctrl_win + SDWB_ADDRESS));
		break;
	default:
		out = 0;
		break;
	}

	mb(); /* ensure serial ordering of non-posted operations for wishbone */

	return out;
}

static void wb_write(struct wishbone *wb, wb_addr_t addr, wb_data_t data)
{
	struct vme_wb_dev *dev;
	unsigned char *reg_win;
	unsigned char *ctrl_win;
	wb_addr_t window_offset;

	dev = container_of(wb, struct vme_wb_dev, wb);
	reg_win = dev->vme_res.map[MAP_REG]->kernel_va;
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

   addr = addr & WBM_ADD_MASK;

	window_offset = addr & WINDOW_HIGH;
	if (window_offset != dev->window_offset) {
		iowrite32(cpu_to_be32(window_offset), ctrl_win + WINDOW_OFFSET_LOW);
		dev->window_offset = window_offset;
	}

   if (unlikely(debug))
      printk(KERN_ALERT VME_WB ": WRITE (0x%x) = 0x%x)\n",
             data, addr);
   iowrite32(cpu_to_be32(data), reg_win + (addr & WINDOW_LOW));
}

static wb_data_t wb_read(struct wishbone *wb, wb_addr_t addr)
{
	wb_data_t out;
	struct vme_wb_dev *dev;
	unsigned char *reg_win;
	unsigned char *ctrl_win;
	wb_addr_t window_offset;

	dev = container_of(wb, struct vme_wb_dev, wb);
	reg_win = dev->vme_res.map[MAP_REG]->kernel_va;
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

   addr = addr & WBM_ADD_MASK;

   window_offset = addr & WINDOW_HIGH;
	if (window_offset != dev->window_offset) {
		iowrite32(cpu_to_be32(window_offset), ctrl_win + WINDOW_OFFSET_LOW);
		dev->window_offset = window_offset;
	}

	out = be32_to_cpu(ioread32(reg_win + (addr & WINDOW_LOW)));

	if (unlikely(debug))
		printk(KERN_ALERT VME_WB ": READ (0x%x) = 0x%x \n", (addr), out);

	mb();
	return out;
}

static int wb_request(struct wishbone *wb, struct wishbone_request *req)
{
	struct vme_wb_dev *dev;
	unsigned char *ctrl_win;
	uint32_t ctrl;

	dev = container_of(wb, struct vme_wb_dev, wb);
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

	ctrl = be32_to_cpu(ioread32(ctrl_win + MASTER_CTRL));
	req->addr = be32_to_cpu(ioread32(ctrl_win + MASTER_ADD));
	req->data = be32_to_cpu(ioread32(ctrl_win + MASTER_DATA));
	req->mask = ctrl & 0xf;
	req->write = (ctrl & 0x40000000) != 0;

	iowrite32(cpu_to_be32(1), ctrl_win + MASTER_CTRL);	/* dequeue operation */

	if (unlikely(debug))
		printk(KERN_ALERT
		       "WB REQUEST:Request ctrl %x addr %x data %x mask %x return %x \n",
		       ctrl, req->addr, req->data, req->mask,
		       (ctrl & 0x80000000) != 0);

	return (ctrl & 0x80000000) != 0;
}

static void wb_reply(struct wishbone *wb, int err, wb_data_t data)
{
	struct vme_wb_dev *dev;
	unsigned char *ctrl_win;

	dev = container_of(wb, struct vme_wb_dev, wb);
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

	iowrite32(cpu_to_be32(data), ctrl_win + MASTER_DATA);
	iowrite32(cpu_to_be32(err + 2), ctrl_win + MASTER_CTRL);

	if (unlikely(debug))
		printk(KERN_ALERT "WB REPLY: pushing data %x reply %x\n", data,
		       err + 2);
}

static void wb_byteenable(struct wishbone *wb, unsigned char be)
{

	struct vme_wb_dev *dev;
	unsigned char *ctrl_win;

	dev = container_of(wb, struct vme_wb_dev, wb);
	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;
	iowrite32(cpu_to_be32(be), ctrl_win + EMUL_DAT_WD);

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

static void init_ctrl_reg(struct vme_wb_dev *dev)
{
	unsigned char *ctrl_win;

	ctrl_win = dev->vme_res.map[MAP_CTRL]->kernel_va;

	iowrite32(0, ctrl_win + EMUL_DAT_WD);
	iowrite32(0, ctrl_win + WINDOW_OFFSET_LOW);
	iowrite32(0, ctrl_win + MASTER_CTRL);

	if (unlikely(debug))
		printk(KERN_ALERT "Initialize Ctrl Register\n");
}

int irq_handler(void *dev_id)
{
	struct vme_wb_dev *dev = dev_id;

	printk(KERN_ALERT "posting MSI!!\n");

	wishbone_slave_ready(&dev->wb);

	return IRQ_HANDLED;
}

int vme_map_window(struct vme_wb_dev *vme_dev, enum vme_map_win map_type)
{
	struct device *dev = vme_dev->vme_dev;
	enum vme_address_modifier am = 0;
	enum vme_data_width dw = 0;
	unsigned long base = 0;
	unsigned int size = 0;
	int rval;
	uint8_t *map_type_c = "";

	if (vme_dev->vme_res.map[map_type] != NULL) {
		dev_err(dev, "Window %d already mapped\n", (int)map_type);
		return -EPERM;
	}

	if (map_type == MAP_REG) {
		am = VME_A32_USER_DATA_SCT;
		dw = VME_D32;
      base = vme_dev->vme_res.slot * 0x10000000;
		size = 0x10000;
		map_type_c = "WB MAP REG";
	} else if (map_type == MAP_CTRL) {
		am = VME_A24_USER_DATA_SCT;
		dw = VME_D32;
		base = vme_dev->vme_res.slot * 0x400;
		size = 0xA0;
		map_type_c = "WB MAP CTRL";
	} else if (map_type == MAP_CR_CSR) {
		am = VME_CR_CSR;
		dw = VME_D32;
		base = vme_dev->vme_res.slot * 0x80000;
		size = 0x80000;
		map_type_c = "WB MAP CS/CSR";
	}

	vme_dev->vme_res.map[map_type] =
	    kzalloc(sizeof(struct vme_mapping), GFP_KERNEL);
	if (!vme_dev->vme_res.map[map_type]) {
		dev_err(dev, "Cannot allocate memory for vme_mapping struct\n");
		return -ENOMEM;
	}

	/* Window mapping */
	vme_dev->vme_res.map[map_type]->am = am;
	vme_dev->vme_res.map[map_type]->data_width = dw;
	vme_dev->vme_res.map[map_type]->vme_addru = 0;
	vme_dev->vme_res.map[map_type]->vme_addrl = base;
	vme_dev->vme_res.map[map_type]->sizeu = 0;
	vme_dev->vme_res.map[map_type]->sizel = size;

	if ((rval = vme_find_mapping(vme_dev->vme_res.map[map_type], 1)) != 0) {
		dev_err(dev, "Failed to map window %d: (%d)\n",
			(int)map_type, rval);
		kfree(vme_dev->vme_res.map[map_type]);
		vme_dev->vme_res.map[map_type] = NULL;
		return -EINVAL;
	}

	dev_info(dev, "%s mapping successful at 0x%p\n",
		 map_type_c, vme_dev->vme_res.map[map_type]->kernel_va);

	return 0;
}

int vme_unmap_window(struct vme_wb_dev *vme_dev, enum vme_map_win map_type)
{
	struct device *dev = vme_dev->vme_dev;

	if (vme_dev->vme_res.map[map_type] == NULL) {
		dev_err(dev, "Window %d not mapped. Cannot unmap\n",
			(int)map_type);
		return -EINVAL;
	}
	if (vme_release_mapping(vme_dev->vme_res.map[map_type], 1)) {
		dev_err(dev, "Unmap for window %d failed\n", (int)map_type);
		return -EINVAL;
	}

	dev_info(dev, "Window %d unmaped\n", (int)map_type);
	kfree(vme_dev->vme_res.map[map_type]);
	vme_dev->vme_res.map[map_type] = NULL;
	return 0;
}

static void vme_csr_write(u8 value, void *base, u32 offset)
{
	offset -= offset % 4;
	iowrite32be(value, base + offset);
}

void vme_setup_csr_fa0(void *base, u32 wb_vme, unsigned vector, unsigned level)
{
	u8 fa[4];		/* FUN0 ADER contents */
	u32 wb_add = wb_vme << 28;
	u32 wb_ctrl_add = wb_vme << 10;

	/* reset the core */
	vme_csr_write(RESET_CORE, base, BIT_SET_REG);
	msleep(10);

	/* disable the core */
	vme_csr_write(ENABLE_CORE, base, BIT_CLR_REG);

	/* default to 32bit WB interface */
	vme_csr_write(WB32, base, WB_32_64);

	/* irq vector */
	vme_csr_write(vector, base, IRQ_VECTOR);

	/* irq level */
	vme_csr_write(level, base, IRQ_LEVEL);

	/*do address relocation for FUN0, WB data mapping */
	fa[0] = (wb_add >> 24) & 0xFF;
	fa[1] = (wb_add >> 16) & 0xFF;
	fa[2] = (wb_add >> 8) & 0xFF;
	fa[3] = (VME_A32_USER_MBLT & 0x3F) << 2;	/* or VME_A32_USER_DATA_SCT */

	vme_csr_write(fa[0], base, FUN0ADER);
	vme_csr_write(fa[1], base, FUN0ADER + 4);
	vme_csr_write(fa[2], base, FUN0ADER + 8);
	vme_csr_write(fa[3], base, FUN0ADER + 12);

	/*do address relocation for FUN1, WB control mapping */
	fa[0] = (wb_ctrl_add >> 24) & 0xFF;
	fa[1] = (wb_ctrl_add >> 16) & 0xFF;
	fa[2] = (wb_ctrl_add >> 8) & 0xFF;
	fa[3] = (VME_A24_USER_MBLT & 0x3F) << 2;	/* or VME_A24_USER_DATA_SCT */

	vme_csr_write(fa[0], base, FUN1ADER);
	vme_csr_write(fa[1], base, FUN1ADER + 4);
	vme_csr_write(fa[2], base, FUN1ADER + 8);
	vme_csr_write(fa[3], base, FUN1ADER + 12);

	/* enable module, hence make FUN0 and FUN1 available */
	vme_csr_write(ENABLE_CORE, base, BIT_SET_REG);
}

static int vme_remove(struct device *pdev, unsigned int ndev)
{
	struct vme_wb_dev *dev = dev_get_drvdata(pdev);

	vme_unmap_window(dev, MAP_CR_CSR);
	vme_unmap_window(dev, MAP_REG);
	vme_unmap_window(dev, MAP_CTRL);
	wishbone_unregister(&dev->wb);
	vme_free_irq(vector_num);
	kfree(dev);

	dev_info(pdev, "removed\n");

	return 0;
}

int vme_is_present(struct vme_wb_dev *vme_dev)
{
	struct device *dev = vme_dev->vme_dev;
	uint32_t idc;
	void *addr;

	addr =
	    vme_dev->vme_res.map[MAP_CR_CSR]->kernel_va + VME_VENDOR_ID_OFFSET;

	idc = be32_to_cpu(ioread32(addr)) << 16;
	idc += be32_to_cpu(ioread32(addr + 4)) << 8;
	idc += be32_to_cpu(ioread32(addr + 8));

	if (idc == VME_VENDOR_ID) {
		dev_info(dev, "vendor ID is 0x%08x\n", idc);
		return 1;
	}

	dev_err(dev, "wrong vendor ID. 0x%08x found, 0x%08x expected\n",
		idc, VME_VENDOR_ID);
	dev_err(dev, "VME not present at slot %d\n", vme_dev->vme_res.slot);

	return 0;

}

static int vme_probe(struct device *pdev, unsigned int ndev)
{
	struct vme_wb_dev *dev;
	const char *name;
	int error = 0;

	if (lun[ndev] >= VME_MAX_DEVICES) {
		dev_err(pdev, "Card lun %d out of range [0..%d]\n",
			lun[ndev], VME_MAX_DEVICES - 1);
		return -EINVAL;
	}

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		dev_err(pdev, "Cannot allocate memory for vme card struct\n");
		return -ENOMEM;
	}

	/* Initialize struct fields */
	dev->vme_res.lun = lun[ndev];
	dev->vme_res.slot = slot[ndev];
	dev->vme_res.vmebase = vmebase[ndev];
	dev->vme_res.vector = vector[ndev];
	dev->vme_res.level = level[ndev];	/* Default value */
	dev->vme_dev = pdev;
	mutex_init(&dev->mutex);
	dev->wb.wops = &wb_ops;
	dev->wb.parent = pdev;
   dev->window_offset  = 0;

	/* Map CR/CSR space */
	error = vme_map_window(dev, MAP_CR_CSR);
	if (error)
		goto failed;

	if (!vme_is_present(dev)) {
		error = -EINVAL;
		goto failed_unmap_crcsr;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
	name = pdev->bus_id;
#else
	name = dev_name(pdev);
#endif

	strlcpy(dev->vme_res.driver, KBUILD_MODNAME,
		sizeof(dev->vme_res.driver));
	snprintf(dev->vme_res.description, sizeof(dev->vme_res.description),
		 "VME at VME-A32 slot %d 0x%08x - 0x%08x irqv %d irql %d",
		 dev->vme_res.slot, dev->vme_res.slot << 19,
		 dev->vme_res.vmebase, vector[ndev], dev->vme_res.level);

	dev_info(pdev, "%s\n", dev->vme_res.description);

	dev_set_drvdata(dev->vme_dev, dev);

	/* configure and activate function 0 */
	vme_setup_csr_fa0(dev->vme_res.map[MAP_CR_CSR]->kernel_va,
			  dev->vme_res.slot, vector[ndev], dev->vme_res.level);

	/* Map WB A32 space */
	error = vme_map_window(dev, MAP_REG);

	if (error)
		goto failed_unmap_wb;

	/* Map WB control A24 space */
	error = vme_map_window(dev, MAP_CTRL);

	if (error)
		goto failed_unmap_wb;

	/* wishbone registration  */
	if (wishbone_register(&dev->wb) < 0) {
		dev_err(pdev, "Could not register wishbone bus\n");
		goto failed_unmap_wb;
	}

	/* register interrupt handler */
	if (vme_request_irq(vector_num, irq_handler, dev, "wb_irq") != 0) {
		printk(KERN_ALERT VME_WB
		       ": could not register interrupt handler\n");
		goto fail_irq;
	}

   init_ctrl_reg(dev);

	return 0;

fail_irq:
	vme_free_irq(vector_num);
failed_unmap_wb:
	vme_unmap_window(dev, MAP_REG);
failed_unmap_crcsr:
	vme_unmap_window(dev, MAP_CR_CSR);
failed:
	kfree(dev);
	return error;
}

static struct vme_driver vme_driver = {
	.probe = vme_probe,
	.remove = vme_remove,
	.driver = {
		   .name = KBUILD_MODNAME,
		   },
};

static int __init vme_init(void)
{
	int error = 0;

	/* Check that all insmod argument vectors are the same length */
	if (lun_num != slot_num || lun_num != vmebase_num ||
	    lun_num != vector_num) {
		pr_err("%s: The number of parameters doesn't match\n",
		       __func__);
		return -EINVAL;
	}

	error = vme_register_driver(&vme_driver, lun_num);
	if (error) {
		pr_err("%s: Cannot register vme driver - lun [%d]\n", __func__,
		       lun_num);
	}

	return error;
}

static void __exit vme_exit(void)
{
	vme_unregister_driver(&vme_driver);
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
