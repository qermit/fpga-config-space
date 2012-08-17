#ifndef __GENSDBFS_H__
#define __GENSDBFS_H__
#include <stdint.h>

#define CFG_NAME "--SDB-CONFIG--"

/* We need to keep track of each file as both unix and sdb entity*/
struct sdbf {
	struct stat stbuf;
	struct dirent de;
	union {
		struct sdb_device s_d;
		struct sdb_interconnect s_i;
		struct sdb_bridge s_b;
	};
	char *fullname;
	unsigned long astart, rstart; /* absolute, relative */
	unsigned long size;
	int nfiles, totsize; /* for dirs */
	struct sdbf *dot; /* for files, pointer to owning dir */
	struct sdbf *parent; /* for dirs, current dir in ../ */
};

static inline uint64_t htonll(uint64_t ll)
{
	uint64_t res;

	if (htonl(1) == 1)
		return ll;
	res = htonl(ll >> 32);
	res |= (uint64_t)(htonl((uint32_t)ll)) << 32;
	return res;
}

#endif /* __GENSDBFS_H__ */
