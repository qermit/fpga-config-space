#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>

#define SPEC_VENDOR 0xbabe
#define SPEC_DEVICE 0xbabe

static int fake_spec_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	/* load wishbone address map firmware */
	/* register wishbone devices */
	return 0;
}

static void fake_spec_remove(struct pci_dev *pdev)
{
}

struct pci_device_id fake_spec_pci_tbl[] = {
	{ SPEC_VENDOR, SPEC_DEVICE, PCI_ANY_ID, 
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
}

module_init(fake_spec_init);
module_exit(fake_spec_exit);
