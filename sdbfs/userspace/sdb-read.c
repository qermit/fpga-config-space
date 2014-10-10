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
static int list_device(struct sdb_device *d, int depth, int base)
{
	struct sdb_product *p;
	struct sdb_component *c;
	struct sdb_synthesis *s;

	unsigned char *data;
	static int warned;
	char *warn;
	int i, ret;

	c = &d->sdb_component;
	p = &c->product;
	s = (void *)d;

	if (!warned && opt_long) {
		fprintf(stderr, "%s: listing format is to be defined\n",
			prgname);
		warned = 1;
	}

	/* Different sdb items are listed in different ways */
	switch(p->record_type) {

	/* The following items are components, and are listed as such */
	case sdb_type_interconnect:
	case sdb_type_device:
	case sdb_type_bridge:
		if (!opt_long) {
			printf("%.19s\n", p->name);
			return 0;
		}


		/* hack: show directory level looking at the internals */
		printf("%016llx:%08x @ %08llx-%08llx ",
		       (long long)ntohll(p->vendor_id), ntohl(p->device_id),
		       (long long)base + ntohll(c->addr_first),
		       (long long)base + ntohll(c->addr_last));
		for (i = 0; i < depth; i++)
			printf("  ");
		printf("%.19s\n", p->name);

		return 0;

	/* A product, but not a component (no address range) */
	case sdb_type_integration:
		if (!opt_long) {
			printf("%.19s\n", p->name);
			return 0;
		}
		printf("%016llx:%08x                     ",
		       (long long)ntohll(p->vendor_id), ntohl(p->device_id));
		/* like above, show directory level */
		for (i = 0; i < depth; i++)
			printf("  ");
		printf("%.19s\n", p->name);
		return 0;

	/* Just a string */
	case sdb_type_repo_url:
		if (opt_long)
			printf("repo-url: %.63s\n",
			       ((struct sdb_repo_url *)d)->repo_url);
		return 0;

	/* Some metadata */
	case sdb_type_synthesis:
		if (!opt_long)
			return 0;
		printf("synthesis-name: %.16s\n", s->syn_name);
		printf("  commit-id: ");
		for (i = 0; i < sizeof(s->commit_id); i++)
			printf("%02x", s->commit_id[i]);
		printf("\n");

		/* Some of the following fields are sometimes empty */
		if (s->tool_name[0] && s->tool_name[0] != ' ')
			printf("  tool-name: %.8s\n", s->tool_name);
		if (s->tool_version)
			printf("  tool-version: 0x%08x\n",
			       ntohl(s->tool_version));
		if (s->date)
			printf("  build-date: %08x\n", ntohl(s->date));
		if (s->user_name[0] && s->tool_name[0] != ' ')
			printf("  build-user: %.15s\n", s->user_name);
		return 0;

	case sdb_type_empty:
		return 0;

	default:
		break;
	}

	/* Unknown record type */
	if (p->record_type & 0x80) {
		warn = "Warning";
		ret = 0;
	} else {
		warn = "Error";
		ret = -1;
	}

	fprintf(stderr, "%s: unknown record type 0x%02x\n", warn,
		p->record_type);
	if (!opt_long) {
		printf("Unknown-record\n");
		return ret;
	}
	/* long listing of unknown record */
	printf("Unknown-record:\n");
	data = (void *)d;
	for (i = 0; i < sizeof(struct sdb_empty); i++)
		printf("%s%02x%c",
		       (i & 0xf) == 0 ? "   " : "",
		       data[i],
		       (i & 0xf) == 0xf ? '\n' : ' ');
	return ret;
}

/* The following three function perform the real work, main() is just glue */
static int do_list(struct sdbfs *fs)
{
	struct sdb_device *d;
	int new = 1;
	int err = 0;

	while ( (d = sdbfs_scan(fs, new)) != NULL) {
		err += list_device(d, fs->depth, fs->base[fs->depth]);
		new = 0;
	}
	return err;
}

static int do_cat_name(struct sdbfs *fs, char *name)
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
	return 0;
}

static int do_cat_id(struct sdbfs *fs, uint64_t vendor, uint32_t dev)
{
	char buf[4096];
	int i;

	i = sdbfs_open_id(fs, htonll(vendor), htonl(dev));
	if (i < 0) {
		fprintf(stderr, "%s: %016llx-%08x: %s\n", prgname,
			(long long)vendor, dev, strerror(-i));
		exit(1);
	}
	while ( (i = sdbfs_fread(fs, -1, buf, sizeof(buf))) > 0)
		fwrite(buf, 1, i, stdout);
	sdbfs_close(fs);
	return 0;
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
	if (opt_verbose)
		fs->flags |= SDBFS_F_VERBOSE;
	err = sdbfs_dev_create(fs);
	if (err) {
		fprintf(stderr, "%s: sdbfs_dev_create(): %s\n", prgname,
			strerror(-err));
		fprintf(stderr, "\t(wrong entry point 0x%08lx?)\n",
			fs->entrypoint);
		exit(1);
	}
	/* Now use the thing: either scan, or look for name, or look for id */
	if (!filearg)
		err = do_list(fs);
	else if (sscanf(filearg, "%llx:%lx", &int64, &int32) != 2)
		err = do_cat_name(fs, filearg);
	else
		err = do_cat_id(fs, int64, int32);

	sdbfs_dev_destroy(fs);
	return err;
}
