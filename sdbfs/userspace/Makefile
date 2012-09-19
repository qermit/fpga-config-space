
AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump

CFLAGS = -Wall -ggdb
CFLAGS += -I../lib -I../include -I../include/linux
LDFLAGS = -L../lib -lsdbfs

PROG = gensdbfs sdb-read

all: $(PROG)

%: %.c
	$(CC) $(CFLAGS) -o $@ $*.c $(LDFLAGS)

$(PROG): ../lib/libsdbfs.a

clean:
	rm -f $(PROG) *.o *~ core
