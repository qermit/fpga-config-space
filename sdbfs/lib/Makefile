
AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

CFLAGS = -Wall -ggdb -O2
CFLAGS += -I../include/linux # for <sdb.h>
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wno-pointer-sign

LIB = libsdbfs.a
OBJS = glue.o access.o

all: $(LIB)

$(OBJS): $(wildcard *.h)

$(LIB): $(OBJS)
	$(AR) r $@ $(OBJS)

clean:
	rm -f $(OBJS) $(LIB) *~ core
