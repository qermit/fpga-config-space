obj-m += wb.o

EXTRA_CFLAGS += -Wall

#KERNEL_DIR=/lib/modules/$(shell uname -r)/build
KERNEL_DIR=/home/mvanga/dev/cern/linux/linux-torvalds

all:
	make -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	make -C $(KERNEL_DIR) M=$(PWD) clean
