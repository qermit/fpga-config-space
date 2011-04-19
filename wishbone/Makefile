include $(PWD)/../paths.mk

obj-m += wb.o

EXTRA_CFLAGS += -Wall

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
