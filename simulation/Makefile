# Trivial top-level Makefile


LINUX ?= $(HOME)/linux-for-wbonesim

# temporarily, use local <linux/wishbone.h>
export WISHBONE_DIR ?= $(shell /bin/pwd)
EXTRA_CFLAGS := -I$(WISHBONE_DIR)/include

obj-m = wishbone.o fakedac.o fakeadc.o fakespec.o 

all:
	make -C $(LINUX) M=$(shell /bin/pwd) modules

clean:
	make -C $(LINUX) M=$(shell /bin/pwd) clean

