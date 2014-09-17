/*
 * Copyright (C) 2012,2014 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "libsdbfs.h"

char *prgname;

int opt_long, opt_verbose, opt_read, opt_entry, opt_mem;
unsigned long opt_memaddr, opt_memsize;

static void help(void)
{
	fprintf(stderr, "%s: Use: \"%s [options] <image-file> [<file>]\n",
		prgname, prgname);
	fprintf(stderr, "   -l          long listing (like ls -l)\n");
	fprintf(stderr, "   -v          verbose\n");
	fprintf(stderr, "   -r          force use of read(2), not mmap(2)\n");
	fprintf(stderr, "   -e <num>    entry point offset\n");
	fprintf(stderr, "   -m <size>@<addr>     memory subset to use\n");
	fprintf(stderr, "   -m <addr>+<size>     memory subset to use\n");
	exit(1);
}

struct sdbr_drvdata {
	void *mapaddr;
	FILE *f;
	unsigned long memaddr;
	unsigned long memsize;
};

/*
 * This read method is needed for non-mmappable files, or stuff that
 * you can't know the size of (e.g., char devices). You can force use of
 * read, to exercise the library procedures, using "-r"
 */
static int do_read(struct sdbfs *fs, int offset, void *buf, int count)
{
	struct sdbr_drvdata *drvdata = fs->drvdata;
	if (opt_verbose)
		fprintf(stderr, "%s @ 0x%08x - size 0x%x (%i)\n", __func__,
			offset, count, count);
	if (drvdata->mapaddr) {
		memcpy(buf, drvdata->mapaddr + offset, count);
		return count;
	}

	/* not mmapped: seek and read */
	if (fseek(drvdata->f, drvdata->memaddr + offset, SEEK_SET) < 0)
		return -1;
	return fread(buf, 1, count, drvdata->f);
}

/* Boring ascii representation of a device */
static void list_device(struct sdb_device *d, int depth, int base)
{
	struct sdb_product *p;
	struct sdb_component *c;
	static int warned;
	int i;

	c = &d->sdb_component;
	p = &c->product;

	if (!opt_long) {
		printf("%.19s\n", p->name);
		return;
	}

	if (!warned) {
		fprintf(stderr, "%s: listing format is to be defined\n",
			prgname);
		warned = 1;
	}

	/* hack: show directory level looking at the internals */
	printf("%016llx:%08x @ %08llx-%08llx ",
	       ntohll(p->vendor_id), ntohl(p->device_id),
	       base + ntohll(c->addr_first), base + ntohll(c->addr_last));
	for (i = 0; i < depth; i++)
		printf("  ");
	printf("%.19s\n", p->name);
}

/* The following three function perform the real work, main() is just glue */
static void do_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;

	while ( (d = sdbfs_scan(fs, new)) != NULL) {
		list_device(d, fs->depth, fs->base[fs->depth]);
		new = 0;
	}
}

static void do_cat_name(struct sdbfs *fs, char *name)
{
	char buf[4096];
	int i;

	i = sdbfs_open_name(fs, name);
	if (i < 0) {
		fprintf(stderr, "%s: %s: %s\n", prgname, name, strerror(-i));
		exit(1);
	}
	while ( (i = sdbfs_fread(fs, -1, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, i, stdout);
	sdbfs_close(fs);
}

static void do_cat_id(struct sdbfs *fs, uint64_t vendor, uint32_t dev)
{
	char buf[4096];
	int i;

	i = sdbfs_open_id(fs, htonll(vendor), htonl(dev));
	if (i < 0) {
		fprintf(stderr, "%s: %016llx-%08x: %s\n", prgname, vendor,
			dev, strerror(-i));
		exit(1);
	}
	while ( (i = sdbfs_fread(fs, -1, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, i, stdout);
	sdbfs_close(fs);
}

/* As promised, here's the user-interface glue (and initialization, I admit) */
int main(int argc, char **argv)
{
	int c, err;
	FILE *f;
	struct sdbfs _fs;
	struct sdbfs *fs = &_fs; /* I like to type "fs->" */
	struct stat stbuf;
	struct sdbr_drvdata *drvdata;
	void *mapaddr;
	char *fsname;
	char *filearg = NULL;
	unsigned long int32;
	unsigned long long int64;
	int pagesize = getpagesize();

	prgname = argv[0];

	while ( (c = getopt(argc, argv, "lvre:m:")) != -1) {
		switch (c) {
		case 'l':
			opt_long = 1;
			break;
		case 'v':
			opt_verbose = 1;
			break;
		case 'r':
			opt_read = 1;
			break;
		case 'e':
			if (sscanf(optarg, "%i", &opt_entry) != 1) {
				fprintf(stderr, "%s: not a number \"%s\"\n",
					prgname, optarg);
				exit(1);
			}
			break;
		case 'm':
			/* memory:  "size@addr",  "addr+size" (blanks ok) */
			if (sscanf(optarg, "%li @ %li", &opt_memsize,
				   &opt_memaddr) == 2)
				break;
			if (sscanf(optarg, "%li + %li", &opt_memaddr,
				   &opt_memsize) == 2)
				break;

			fprintf(stderr, "%s: \"%s\" must be <size>@<addr> "
				"or <addr>+<size>\n", prgname, optarg);
				exit(1);
			break;
		}
	}
	if (optind < argc - 2 || optind > argc - 1)
		help();

	fsname = argv[optind];
	if (optind + 1 < argc)
		filearg = argv[optind + 1];
	if ( !(f = fopen(fsname, "r")) || fstat(fileno(f), &stbuf) < 0) {
		fprintf(stderr, "%s: %s: %s\n", prgname, fsname,
			strerror(errno));
		exit(1);
	}

	stbuf.st_size += pagesize - 1;
	stbuf.st_size &= ~(pagesize - 1);
	mapaddr = mmap(0,
		       opt_memsize ? opt_memsize : stbuf.st_size,
		       PROT_READ, MAP_PRIVATE, fileno(f),
		       opt_memaddr /* 0 by default */);
	if (mapaddr == MAP_FAILED)
		mapaddr = NULL; /* We'll seek/read */

	/* So, describe the filesystem instance and give it to the library */
	memset(fs, 0, sizeof(*fs));

	drvdata = calloc(1, sizeof(*drvdata));
	if (!drvdata) {perror("malloc"); exit(1);}
	drvdata->f = f;
	drvdata->memaddr = opt_memaddr;
	drvdata->memsize = opt_memsize;
	drvdata->mapaddr = mapaddr;

	fs->drvdata = drvdata;
	fs->name = fsname; /* not mandatory */
	fs->blocksize = 256; /* only used for writing, actually */
	fs->entrypoint = opt_entry;
	if (opt_read || !drvdata->mapaddr)
		fs->read = do_read;
	else
		fs->data = mapaddr;

	err = sdbfs_dev_create(fs, opt_verbose);
	if (err) {
		fprintf(stderr, "%s: sdbfs_dev_create(): %s\n", prgname,
			strerror(-err));
		fprintf(stderr, "\t(wrong entry point 0x%08lx?)\n",
			fs->entrypoint);
		exit(1);
	}
	/* Now use the thing: either scan, or look for name, or look for id */
	if (!filearg)
		do_list(fs);
	else if (sscanf(filearg, "%llx:%lx", &int64, &int32) != 2)
		do_cat_name(fs, filearg);
	else
		do_cat_id(fs, int64, int32);

	sdbfs_dev_destroy(fs);
	return 0;
}
