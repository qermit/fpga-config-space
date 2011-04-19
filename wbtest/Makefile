include $(PWD)/../paths.mk

obj-m += wbtest.o

EXTRA_CFLAGS += -I$(PWD)/../wishbone

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
