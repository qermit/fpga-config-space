ifneq ($(KERNELRELEASE),)
# When Makefile is invoked from kernel build system, define objects

ccflags-y += -DGIT_REVISION="$(shell git --git-dir=$(src)/../.git log -n1 --pretty='format:%h (%ci)')"

obj-m += pcie_wb.o wishbone.o spec_wb.o
# Only build wishbone-serial if it was missing
ifneq ($(CONFIG_USB_SERIAL_WISHBONE),m)
ifneq ($(CONFIG_USB_SERIAL_WISHBONE),y)
obj-m += wishbone-serial.o
endif
endif

else
# When the Makefile is run directly, invoke the kernel build system

KERNELVER ?= `uname -r`
KERNELDIR ?= /lib/modules/$(KERNELVER)/build
PWD       := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD)

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_PATH=$(STAGING) modules_install
	mkdir -p $(STAGING)/etc/udev/rules.d
	cp 95-pcie-wb.rules $(STAGING)/etc/udev/rules.d
	[ $$(id -u) -ne 0 ] || depmod -a
	[ $$(id -u) -ne 0 ] || groupadd wishbone || true

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif
