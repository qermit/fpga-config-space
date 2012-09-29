/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __SDBFS_INT_H__
#define __SDBFS_INT_H__
#include <linux/fs.h>
#include <linux/sdb.h>

struct sdbfs_inode {
	union {
		struct sdb_device s_d;
		struct sdb_interconnect s_i;
		struct sdb_bridge s_b;
	};
	struct sdb_device *files; /* Only used for directories */
	struct inode ino;
};

/* This is needed to convert endianness. Hoping it is not defined elsewhere */
static inline uint64_t htonll(uint64_t ll)
{
        uint64_t res;

        if (htonl(1) == 1)
                return ll;
        res = htonl(ll >> 32);
        res |= (uint64_t)(htonl((uint32_t)ll)) << 32;
        return res;
}
static inline uint64_t ntohll(uint64_t ll)
{
	return htonll(ll);
}

#endif /* __SDBFS_INT_H__ */
