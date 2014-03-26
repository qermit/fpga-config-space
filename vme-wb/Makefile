REPO_VME  = git://ohwr.org/hdl-core-lib/vme64x-core/legacy-vme64x-core.git
DRV_DIR = $(shell pwd)/../../legacy-vme64x-core
SRC_NEWK := 3.10

ifneq ($(KERNELRELEASE),)

KERNEL_LIMIT := 3.10.0
TEMP 	 := $(shell /bin/echo "$(KERNELVERSION)'\'n$(KERNEL_LIMIT)")
KERNEL := $(shell /bin/echo -e $(TEMP) | sed '/^$$/d' | sort -t. -k1,1nr -k2,2nr | head -1)

# If the user did not specify otherwise, use the in-kernel VME driver iff it was enabled
ifeq ($(VME_SOURCE),)
VME_SOURCE=external
ifeq ($(CONFIG_VME_BUS),y)
VME_SOURCE=inkernel
endif
ifeq ($(CONFIG_VME_BUS),m)
VME_SOURCE=inkernel
endif
endif

ifneq ($(VME_SOURCE),inkernel) # external
ifneq ($(KERNEL),$(KERNEL_LIMIT))
$(error Cannot build external VME driver with 3.10 - enable in-tree VME $(KERNEL) $(KERNEL_LIMIT))
endif
endif

ifneq ($(VME_SOURCE),external) # internal
ifneq ($(CONFIG_VME_BUS),y)
ifneq ($(CONFIG_VME_BUS),m)
$(warn Kernel must have in-tree VME feature enabled)
endif
endif
endif

KBUILD_EXTRA_SYMBOLS += $(M)/../pcie-wb/Module.symvers
ifeq ($(VME_SOURCE),external)
KBUILD_EXTRA_SYMBOLS += $(M)/../../legacy-vme64x-core/drv/driver/Module.symvers
obj-m += vme_wb.o
$(obj)/vme_wb.c::
	$(MAKE) -C $(obj)/../../legacy-vme64x-core/drv/driver KERNELDIR="$(CURDIR)"
else	
obj-m += $(SRC_NEWK)/vme_wb.o
endif

ccflags-y += -I$(M)/../../legacy-vme64x-core/drv/driver
ccflags-y += -I$(M)/../pcie-wb

else

KERNELVER ?= $(shell /bin/uname -r)
KERNELDIR ?= /lib/modules/$(KERNELVER)/build
PWD       := $(shell pwd)

all:	gitmodules wishbone modules

gitmodules:
	@test -d $(DRV_DIR) &&  \
	echo "Driver source code found" || \
	git clone $(REPO_VME) $(DRV_DIR)

wishbone:
	$(MAKE) -C ../pcie-wb KERNELDIR="$(KERNELDIR)"

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

install:
	test ! -f ../../legacy-vme64x-core/drv/driver/vmebus.ko || \
		$(MAKE) -C ../../legacy-vme64x-core/drv/driver install
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(STAGING) modules_install

clean: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	$(MAKE) -C ../../legacy-vme64x-core/drv/driver clean
endif
