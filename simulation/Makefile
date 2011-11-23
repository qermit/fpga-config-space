# Trivial top-level Makefile


#LINUX ?= $(HOME)/linux-for-wbonesim
CPU?=L865
KVER?=2.6.24.7-rt27
LINUX?=/acc/sys/$(CPU)/usr/src/kernels/$(KVER)

# temporarily, use local <linux/wishbone.h>
EXTRA_CFLAGS := -I$(obj)/include

obj-m = wishbone.o fakedac.o fakeadc.o fake_wb_bus.o 

all:
	make -C $(LINUX) M=$(shell /bin/pwd) modules

clean:
	make -C $(LINUX) M=$(shell /bin/pwd) clean

