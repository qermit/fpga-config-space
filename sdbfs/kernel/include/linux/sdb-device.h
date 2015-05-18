/*
 * Copyright (C) 2015 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */
#ifndef __LINUX_SDB_DEV_H__
#define __LINUX_SDB_DEV_H__
#include <linux/sdb.h>

struct fmc_device;
struct fmc_driver;

/*
 * This bus abstraction is developed separately from drivers, so we need
 * to check the version of the data structures we receive.
 */
#define FMC_MAJOR       1
#define FMC_MINOR       0
#define FMC_VERSION     ((FMC_MAJOR << 16) | FMC_MINOR)
#define __FMC_MAJOR(x)  ((x) >> 16)
#define __FMC_MINOR(x)  ((x) & 0xffff)

/*
 * Identification can be based on vendor/device, on class, abi and so on
 */

#endif /* __LINUX_SDB_DEV_H__ */
