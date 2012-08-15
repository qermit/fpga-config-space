LINUX ?= /lib/modules/$(shell uname -r)/build

# get sdb.h from ../include/linux/
ccflags-y = -I$M/../include

obj-m = sdbfs.o
obj-m += sdb-fakedev.o

sdbfs-y = sdbfs-core.o
sdbfs-y += sdbfs-file.o
sdbfs-y += sdbfs-devmem.o
sdbfs-y += sdbfs-device.o

all: modules

install: modules_install

modules clean modules_install:
	$(MAKE) -C $(LINUX) M=$(shell /bin/pwd) $@
