CFLAGS += -I../include -Wall -Wextra

all: wb_cfggen

wb_cfggen: wb_cfggen.o
	$(CC) $(CFLAGS) wb_cfggen.o -o wb_cfggen

clean:
	rm -rf wb_cfggen *.o
