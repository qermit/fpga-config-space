#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/firmware.h>

#include "../wishbone/wb.h"

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
struct wb_device **wbdev;

int n = 0;

static int fake_spec_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int i = -1;
	int j = 0;
	struct wb_header *header;
	const struct firmware *wb_fw;
	char fwname[64];
	if (n)
		return -1;
	n = 1;
	/* load wishbone address map firmware */
	sprintf(fwname, "fakespec-%04x-%04x", spec_vendor, spec_device);
	if (request_firmware(&wb_fw, fwname, &pdev->dev)) {
		printk(KERN_ERR "failed to load firmware\n");
		return -1;
	}
	printk(KERN_INFO "fakespec: loaded firmware\n");
	if (wb_fw->size % 1024)
		printk(KERN_DEBUG "not aligned to 1024 bytes. skipping extra\n");
	ndev = wb_fw->size / 1024;
	if (!ndev) {
		printk(KERN_DEBUG "no devices in memory map\n");
		goto nodev;
	}
	wbdev = kzalloc(sizeof(struct wb_device *) * ndev, GFP_KERNEL);
	if (!wbdev)
		goto arr_alloc_fail;
	/* register wishbone devices */
	while (++i < ndev) {
		header = (struct wb_header *)&wb_fw->data[i * 1024];
		if (!header->vendor)
			continue;
		wbdev[j] = kzalloc(sizeof(struct wb_device), GFP_KERNEL);
		if (!wbdev[j])
			goto alloc_fail;
		wbdev[j]->vendor = header->vendor;
		wbdev[j]->device = header->device;
		wbdev[j]->subdevice = header->subdevice;
		wbdev[j]->flags = header->flags;
		if (wb_register_device(wbdev[j]) < 0)
			goto register_fail;
		j++;
	}
	ndev = j;
	printk("fakespec: found %d devices\n", ndev);
	for (i = 0; i < ndev; i++)
		printk("fakespec: device: %d %d %d %08x\n", wbdev[i]->vendor, wbdev[i]->device, wbdev[i]->subdevice, wbdev[i]->flags);
	return 0;

nodev:
	release_firmware(wb_fw);
	return 0;

register_fail:
	kfree(wbdev[j]);
alloc_fail:
	printk("dodo\n");
	while (--j >= 0) {
		wb_unregister_device(wbdev[j]);
		kfree(wbdev[j]);
	}
	kfree(wbdev);
arr_alloc_fail:
	release_firmware(wb_fw);
	return -1;
}

static void fake_spec_remove(struct pci_dev *pdev)
{
	int i;
	printk("ndev: %d\n", ndev);
	for (i = 0; i < ndev; i++) {
		printk("unregistering device %d\n", i);
		wb_unregister_device(wbdev[i]);
		printk("done\n");
		kfree(wbdev[i]);
	}
	kfree(wbdev);
	printk("finished\n");
}

struct pci_device_id fake_spec_pci_tbl[] = {
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, 
	  PCI_ANY_ID, 0, 0, 0},
	{ 0, },
};

struct pci_driver fake_spec_pci_driver = {
	.name = "fake-spec",
	.id_table = fake_spec_pci_tbl,
	.probe = fake_spec_probe,
	.remove = __devexit_p(fake_spec_remove),
};

int fake_spec_init(void)
{
	return pci_register_driver(&fake_spec_pci_driver);
}

void fake_spec_exit(void)
{
	pci_unregister_driver(&fake_spec_pci_driver);
}

module_init(fake_spec_init);
module_exit(fake_spec_exit);

MODULE_LICENSE("GPL");
