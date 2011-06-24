#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/firmware.h>
#include <linux/mutex.h>

#include "wb.h"
#include "sdwb.h"

#define PFX "fakespec: "

int spec_vendor = 0xbabe;
int spec_device = 0xbabe;
module_param(spec_vendor, int, S_IRUGO);
module_param(spec_device, int, S_IRUGO);

static int ndev;

LIST_HEAD(spec_devices);
static struct mutex list_lock;

static int n;

static int fake_spec_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	char fwname[64];
	unsigned int header_addr = 0; /* Normally read from the BAR */
	struct sdwb_head *header;
	struct sdwb_wbid *id;
	struct sdwb_wbd *wbd;
	struct wb_device *wbdev, *next;
	const struct firmware *wb_fw;

	/* Horrible way of ensuring single probe call. Sorry for race conds */
	if (n)
		return -ENODEV;
	n = 1;

	/*
	 * load firmware with wishbone address map. In the real driver,
	 * we would first load the bitstream into the fpga and then read
	 * the header from its appropriate location.
	 *
	 * For loading the bitstream, we read the bitstream ID off
	 * the eeprom on the spec board? Or some other way?
	 *
	 * Below, we just use the PCI id to get the firmware file.
	 */
	sprintf(fwname, "fakespec-%08x-%04x", spec_vendor, spec_device);
	if (request_firmware(&wb_fw, fwname, &pdev->dev)) {
		printk(KERN_ERR PFX "failed to load firmware\n");
		return -1;
	}

	header = (struct sdwb_head *)&wb_fw->data[header_addr];
	if (header->magic != SDWB_HEAD_MAGIC) {
		printk(KERN_ERR PFX "invalid sdwb header\n");
		goto head_fail;
	}

	id = (struct sdwb_wbid *)&wb_fw->data[header->wbid_address];
	printk(KERN_INFO PFX "found sdwb ID: %lld\n", id->bstream_type);

	wbd = (struct sdwb_wbd *)&wb_fw->data[header->wbd_address];
	while (wbd->wbd_magic == SDWB_WBD_MAGIC) {
		wbdev = kzalloc(sizeof(struct wb_device), GFP_KERNEL);
		if (!wbdev)
			goto alloc_fail;
		memcpy(&wbdev->wbd, wbd, sizeof(struct sdwb_wbd));
		if (wb_register_device(wbdev) < 0)
			goto register_fail;
		mutex_lock(&list_lock);
		list_add(&wbdev->list, &spec_devices);
		ndev++;
		mutex_unlock(&list_lock);
		wbd++;
	}
	printk(KERN_INFO PFX "found %d wishbone devices\n", ndev);
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

head_fail:
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
	ndev = 0;
	mutex_init(&list_lock);
	return pci_register_driver(&fake_spec_pci_driver);
}

static void fake_spec_exit(void)
{
	pci_unregister_driver(&fake_spec_pci_driver);
}

module_init(fake_spec_init);
module_exit(fake_spec_exit);

MODULE_AUTHOR("Manohar Vanga <mvanga@cern.ch>");
MODULE_DESCRIPTION("Fake Spec board driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("Apr 2011");
