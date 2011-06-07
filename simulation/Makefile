# Trivial top-level Makefile


LINUX ?= $(HOME)/linux-for-wbonesim

obj-m = wb.o fakedac.o fakeadc.o fakespec.o 

all:
	make -C $(LINUX) M=$(shell /bin/pwd) modules

clean:
	make -C $(LINUX) M=$(shell /bin/pwd) clean

