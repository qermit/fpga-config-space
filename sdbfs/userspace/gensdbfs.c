/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Alessandro Rubini <rubini@gnudd.com>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 *
 * This work is part of the White Rabbit project, a research effort led
 * by CERN, the European Institute for Nuclear Research.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <linux/sdb.h>

/*
 * This takes a directory and turns it into an sdb image. An optional
 * config file (called --SDB-CONFIG--) states more about the entries.
 * Information about the storage, on the other hand, is received on
 * the command line.
 */

static int usage(char *prgname)
{
	fprintf(stderr, "%s: Use \"%s [<options>] <inputdir> <output>\"\n",
		prgname, prgname);
	fprintf(stderr, "  -b <number> : block size (default 64)\n");
	fprintf(stderr, "  -s <number> : device size (default: as needed)\n");
	fprintf(stderr, "  a file called \"--SDB-CONFIG--\", in the root of\n"
		"  <inputdir> is used as configuration file\n");
	exit(1);
}

/* We need to keep track of each file as both unix and sdb entity*/
struct sdbf {
	struct stat stbuf;
	union {
		struct sdb_device s_d;
		struct sdb_interconnect s_i;
		struct sdb_bridge s_b;
	};
	unsigned long astart, rstart; /* absolute, relative */
	unsigned long size;
	int nfiles, totsize; /* for dirs */
	struct sdbf *dot; /* for files, pointer to owning dir */
	struct sdbf *parent; /* for dirs, current dir in ../ */
};

/* Lazily, these are globals, pity me */
static unsigned blocksize = 64;
static unsigned long devsize = 0; /* unspecified */

/* All these functions return a tree, or NULL on error, after printing msg */
static struct sdbf *scan_input(char *name, FILE **cfgf)
{
	return NULL;
}

static void dump_tree(struct sdbf *tree)
{}

static struct sdbf *scan_config(struct sdbf *tree, FILE *f)
{
	return NULL;
}

static struct sdbf *alloc_storage(struct sdbf *tree)
{
	return NULL;
}

static struct sdbf *write_sdb(struct sdbf *tree)
{
	return NULL;
}


int main(int argc, char **argv)
{
	int c;
	struct stat stbuf;
	FILE *f;
	char *rest;
	struct sdbf *tree;

	while ( (c = getopt(argc, argv, "b:s:")) != -1) {
		switch (c) {
		case 'b':
			blocksize = strtol(optarg, &rest, 0);
			if (rest && *rest) {
				fprintf(stderr, "%s: not a number \"%s\"\n",
					argv[0], optarg);
				exit(1);
			}
			break;
		case 's':
			devsize = strtol(optarg, &rest, 0);
			if (rest && *rest) {
				fprintf(stderr, "%s: not a number \"%s\"\n",
					argv[0], optarg);
				exit(1);
			}
			break;
		}
	}
	if (optind != argc - 2)
		usage(argv[0]);

	/* check input and output */
	if (stat(argv[optind], &stbuf) < 0) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[optind],
			strerror(errno));
		exit(1);
	}
	if (!S_ISDIR(stbuf.st_mode)) {
		fprintf(stderr, "%s: %s: not a directory\n", argv[0],
			argv[optind]);
		exit(1);
	}
	f = fopen(argv[optind+1], "w");
	if (!f) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], argv[optind+1],
			strerror(errno));
		exit(1);
	}
	fclose(f);

	/* scan the whole input tree and save the information */
	tree = scan_input(argv[optind], &f /* config file */);
	if (!tree)
		exit(1);
	if (getenv("VERBOSE"))
		dump_tree(tree);

	/* read configuration file and save its info for each file */
	if (f)
		tree = scan_config(tree, f);
	if (!tree)
		exit(1);

	/* allocate space in the storage */
	tree = alloc_storage(tree);
	if (!tree)
		exit(1);

	/* write out each file */
	tree = write_sdb(tree);
	if (!tree)
		exit(1);

	exit(0);
}
