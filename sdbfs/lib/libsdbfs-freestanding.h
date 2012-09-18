
#error "freestanding support for libsdbfs is not yet available"

/* Though freestanding, some minimal headers are expected to exist */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SDB_KERNEL	0
#define SDB_USER	0
#define SDB_FREESTAND	1
