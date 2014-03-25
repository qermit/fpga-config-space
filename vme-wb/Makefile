REPO_VME  = git://ohwr.org/hdl-core-lib/vme64x-core/legacy-vme64x-core.git
DRV_DIR = $(shell pwd)/../../legacy-vme64x-core
KERNEL_LIMIT := 3.10.0
SRC_NEWK := 3.10

KERNELVER ?= $(shell /bin/uname -r)
TEMP 	 := $(shell /bin/echo "$(KERNELVER)'\'n$(KERNEL_LIMIT)")
KERNEL := $(shell /bin/echo -e $(TEMP) | sed '/^$$/d' | sort -t. -k1,1nr -k2,2nr | head -1)


ifeq ($(KERNEL),$(KERNEL_LIMIT))		
	SUB := gitmodules wishbone vme modules
	export CONFIG_VME_OLDK=m
else	
	SUB := wishbone modules
	export CONFIG_VME_NEWK=m
endif

ifneq ($(KERNELRELEASE),)

KBUILD_EXTRA_SYMBOLS += $(M)/../pcie-wb/Module.symvers 

ifeq ($(CONFIG_VME_OLDK),m)
	KBUILD_EXTRA_SYMBOLS += $(M)/../../legacy-vme64x-core/drv/driver/Module.symvers
endif

obj-$(CONFIG_VME_NEWK) := $(SRC_NEWK)/vme_wb.o
obj-$(CONFIG_VME_OLDK)	:= vme_wb.o

ccflags-y := -I$(M)/../../legacy-vme64x-core/drv/driver
ccflags-y += -I$(M)/../pcie-wb

else

KERNELDIR ?= /lib/modules/$(KERNELVER)/build
PWD       ?= $(shell pwd)

all: $(SUB) 

gitmodules:
	@test -d $(DRV_DIR) &&  \
	echo "Driver source code found" || \
	git clone $(REPO_VME) $(DRV_DIR)

wishbone:
	$(MAKE) -C ../pcie-wb KERNELVER="$(KERNELVER)" KERNELDIR="$(KERNELDIR)"

vme:
	$(MAKE) -C ../../legacy-vme64x-core/drv/driver KERNELVER="$(KERNELVER)" KERNELDIR="$(KERNELDIR)"

modules:
	@echo $(KBUILD_EXTRA_SYMBOLS)
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(STAGING) modules_install

clean: 
	rm -rf *~ core *.depend .*.cmd *.ko .*.mod.c .tmp_versions 
	rm -rf ./$(SRC_NEWK)/*.*o*
endif
