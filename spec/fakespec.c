#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/mutex.h>

#include "../wishbone/wb.h"

#define PFX "fakespec: "

int spec_vendor = 0xbabe;
int spec_device = 0xbabe;
module_param(spec_vendor, int, S_IRUGO);
module_param(spec_device, int, S_IRUGO);

struct wb_header {
	__u32 vendor;
	__u16 device;
	__u16 subdevice;
	__u32 flags;
};

int ndev;

LIST_HEAD(spec_devices);
static struct mutex list_lock;

static int n;

static int fake_spec_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int i = -1;
	int j = 0;
	int nblock;
	char fwname[64];
	struct wb_header *header;
	struct wb_device *wbdev, *next;
	const struct firmware *wb_fw;

	/* Horrible way of ensuring single probe call. Sorry for race conds */
	if (n)
		return -1;
	n = 1;

	/*
	 * load firmware with wishbone address map. In the real driver, we would
	 * first load the bitstream into the fpga and then walk through the
	 * address space.
	 */
	sprintf(fwname, "fakespec-%08x-%04x", spec_vendor, spec_device);
	if (request_firmware(&wb_fw, fwname, &pdev->dev)) {
		printk(KERN_ERR PFX "failed to load firmware\n");
		return -1;
	}

	/* print a warning if it is not aligned to 1KB blocks */
	if (wb_fw->size % 1024)
		printk(KERN_DEBUG PFX "not aligned to 1024 bytes. skipping extra\n");

	/* find the number of block present */
	nblock = wb_fw->size / 1024;
	if (!nblock) {
		printk(KERN_DEBUG PFX "no devices in memory map\n");
		goto nodev;
	}

	/* register wishbone devices */
	while (++i < nblock) {
		header = (struct wb_header *)&wb_fw->data[i * 1024];
		if (!header->vendor)
			continue;
		wbdev = kzalloc(sizeof(struct wb_device), GFP_KERNEL);
		if (!wbdev)
			goto alloc_fail;
		wbdev->vendor = header->vendor;
		wbdev->device = header->device;
		wbdev->subdevice = header->subdevice;
		wbdev->flags = header->flags;
		if (wb_register_device(wbdev) < 0)
			goto register_fail;
		mutex_lock(&list_lock);
		list_add(&wbdev->list, &spec_devices);
		mutex_unlock(&list_lock);
		j++;
	}
	ndev = j;
	printk(KERN_INFO PFX "found %d wishbone devices\n", ndev);
	return 0;

nodev:
	release_firmware(wb_fw);
	return 0;

register_fail:
	kfree(wbdev);
alloc_fail:
	mutex_lock(&list_lock);
	list_for_each_entry_safe(wbdev, next, &spec_devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
		kfree(wbdev);
	}
	mutex_unlock(&list_lock);
	release_firmware(wb_fw);
	return -1;
}

static void fake_spec_remove(struct pci_dev *pdev)
{
	struct wb_device *wbdev, *next;
	mutex_lock(&list_lock);
	list_for_each_entry_safe(wbdev, next, &spec_devices, list) {
		list_del(&wbdev->list);
		wb_unregister_device(wbdev);
		kfree(wbdev);
	}
	mutex_unlock(&list_lock);
}

static DEFINE_PCI_DEVICE_TABLE(fake_spec_pci_tbl) = {
	{ PCI_DEVICE(PCI_ANY_ID, PCI_ANY_ID) },
	{ 0, 0, 0, 0, 0, 0, 0 },
};
MODULE_DEVICE_TABLE(pci, fake_spec_pci_tbl);

static struct pci_driver fake_spec_pci_driver = {
	.name = "fake-spec",
	.id_table = fake_spec_pci_tbl,
	.probe = fake_spec_probe,
	.remove = __devexit_p(fake_spec_remove),
};

static int fake_spec_init(void)
{
	n = 0;
	mutex_init(&list_lock);
	return pci_register_driver(&fake_spec_pci_driver);
}

static void fake_spec_exit(void)
{
	pci_unregister_driver(&fake_spec_pci_driver);
}

module_init(fake_spec_init);
module_exit(fake_spec_exit);

MODULE_LICENSE("GPL");
