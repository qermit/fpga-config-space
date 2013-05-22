#ifndef __GENSDBFS_H__
#define __GENSDBFS_H__
#include <stdint.h>

#define CFG_NAME "--SDB-CONFIG--"
#define DEFAULT_VENDOR htonll(0x46696c6544617461LL) /* "FileData" */

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
	char *basename;
	unsigned long ustart, rstart;	/* user (mandated), relative */
	unsigned long base, size;	/* base is absolute, for output */
	int nfiles, totsize;		/* for dirs */
	struct sdbf *dot;		/* for files, pointer to owning dir */
	struct sdbf *parent;		/* for dirs, current dir in ../ */
	struct sdbf *subdir;		/* for files that are dirs */
	int level;			/* subdir level */
	int userpos;			/* only allowed at level 0 */
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

#define ntohll htonll

#endif /* __GENSDBFS_H__ */
