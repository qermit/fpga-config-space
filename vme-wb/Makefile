REPO_VME  = git://ohwr.org/hdl-core-lib/vme64x-core/legacy-vme64x-core.git
DRV_DIR = $(shell pwd)/../../legacy-vme64x-core


ifneq ($(KERNELRELEASE),)
KBUILD_EXTRA_SYMBOLS += $(M)/../pcie-wb/Module.symvers $(M)/../../legacy-vme64x-core/drv/driver/Module.symvers	
	obj-m	:= vme_wb.o
	ccflags-y := -I$(M)/../../legacy-vme64x-core/drv/driver
	ccflags-y += -I$(M)/../pcie-wb
else

KERNELVER ?= `uname -r`
KERNELDIR ?= /lib/modules/$(KERNELVER)/build
PWD       := $(shell pwd)

all: gitmodules wishbone vme modules

gitmodules:
	@test -d $(DRV_DIR) &&  \
	echo "Driver source code found" || \
	git clone $(REPO_VME) $(DRV_DIR)

wishbone:
	$(MAKE) -C ../pcie-wb KERNELVER="$(KERNELVER)" KERNELDIR="$(KERNELDIR)"

vme:
	$(MAKE) -C ../../legacy-vme64x-core/drv/driver KERNELVER="$(KERNELVER)" KERNELDIR="$(KERNELDIR)"

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(STAGING) modules_install

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

endif
