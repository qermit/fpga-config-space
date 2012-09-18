
#error "kernel support for libsdbfs is not yet available"

#include <linux/types.h>
#include <linux/string.h>
#include <linux/errno.h>

#define SDB_KERNEL	1
#define SDB_USER	0
#define SDB_FREESTAND	0


#define sdb_print(format, ...) printk(format, __VA_ARGS__)
