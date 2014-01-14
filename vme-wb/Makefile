# This is useful if cross-compiling. Taken from kernel Makefile (CC changed)
#AS      =$(CROSS_COMPILE)as
#LD      =$(CROSS_COMPILE)ld
#CC      =$(CROSS_COMPILE)gcc
#CPP     =$(CC) -E
#AR      =$(CROSS_COMPILE)ar
#NM      =$(CROSS_COMPILE)nm
#STRIP   =$(CROSS_COMPILE)strip
#OBJCOPY =$(CROSS_COMPILE)objcopy
#OBJDUMP =$(CROSS_COMPILE)objdump

KERNELVER ?= `uname -r`
KERNELDIR ?= /lib/modules/$(KERNELVER)/build
REPO_VME  = git://ohwr.org/hdl-core-lib/vme64x-core/legacy-vme64x-core.git
DRV_DIR = $(shell pwd)/../../legacy-vme64x-core


ifneq ($(KERNELRELEASE),)
	obj-m	:= vme_wb.o
	ccflags-y := -I$(M)/../../legacy-vme64x-core/drv/driver
	ccflags-y += -I$(M)/../pcie-wb
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD       := $(shell pwd)

#KBUILD_EXTMOD :=  $(PWD)/../pcie-wb/Module.symvers
#KBUILD_EXTRA_SYMBOLS += $(M)/Module.symvers.vme

all: gitmodules wishbone modules

gitmodules:
	@test -d $(DRV_DIR) &&  \
	echo "Driver source code found" || \
	git clone $(REPO_VME) $(DRV_DIR)

wishbone:
	$(MAKE) -C ../pcie-wb

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

