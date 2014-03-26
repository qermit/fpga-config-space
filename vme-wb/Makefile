KERNELVER ?= $(shell /bin/uname -r)
KERNELDIR ?= /lib/modules/$(KERNELVER)/build
PWD       := $(shell pwd)

all:	wishbone modules

wishbone:
	$(MAKE) -C ../pcie-wb KERNELDIR="$(KERNELDIR)"

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(STAGING) modules_install

clean: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(MAKE) -C ../legacy-vme64x-core/drv/driver clean
