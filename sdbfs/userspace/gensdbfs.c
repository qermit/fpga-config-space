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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <linux/sdb.h>
#include "gensdbfs.h"

/*
 * This takes a directory and turns it into an sdb image. An optional
 * config file (called --SDB-CONFIG--) states more about the entries.
 * Information about the storage, on the other hand, is received on
 * the command line.
 */

/* Lazily, these are globals, pity me */
static unsigned blocksize = 64;
static unsigned long devsize = 0; /* unspecified */
static unsigned long lastwritten = 0;
static char *prgname;

static inline unsigned long SDB_ALIGN(unsigned long x)
{
	return (x + (blocksize - 1)) & ~(blocksize - 1);
}

/* Helpers for scan_input(), which is below */
static void __fill_dot(struct sdbf *dot)
{
	struct sdb_interconnect *i = &dot->s_i;
	struct sdb_component *c = &i->sdb_component;
	struct sdb_product *p = &c->product;

	dot->fullname = strdup(".");
	i->sdb_magic = htonl(SDB_MAGIC);
	i->sdb_version = 1;
	i->sdb_bus_type = sdb_data;
	/* c->addr_first/last to be filled later */
	p->vendor_id = htonll(0x46696c6544617461LL); /* "FileData" */
	p->version = htonl(1); /* FIXME: version of gensdbfs */
	/* FIXME: date */
	memset(p->name, ' ', sizeof(p->name));
	p->name[0] = '.';
	memcpy(&p->device_id, p->name, 4);
	p->record_type = sdb_type_interconnect;
}

static int __fill_file(struct sdbf *f, char *dir, char *fname)
{
	char fn[PATH_MAX];
	struct sdb_device *d = &f->s_d;
	struct sdb_component *c = &d->sdb_component;
	struct sdb_product *p = &c->product;
	int flags, len;

	strcpy(fn, dir);
	strcat(fn, "/");
	strcat(fn, fname);
	f->fullname = strdup(fn);
	if (stat(fn, &f->stbuf) < 0) {
		fprintf(stderr, "%s: stat(%s): %s\n", prgname, fn,
			strerror(errno));
		return -1;
	}
	if (!S_ISREG(f->stbuf.st_mode)) {
		/* FIXME: support subdirs */
		fprintf(stderr, "%s: ignoring non-regular \"%s\"\n",
			prgname, fn);
		return 0;
	}
	/*
	 * size can be enlarged by config file, but in any case if the
	 * file can be written to, align to the block size
	 */
	f->size = f->stbuf.st_size;
	if (f->stbuf.st_mode & S_IWOTH) f->size = SDB_ALIGN(f->size);
	/* abi fields remain 0 */
	flags = 0;
	if (f->stbuf.st_mode & S_IROTH) flags |= SDB_DATA_READ;
	if (f->stbuf.st_mode & S_IWOTH) flags |= SDB_DATA_WRITE;
	if (f->stbuf.st_mode & S_IXOTH) flags |= SDB_DATA_EXEC;
	d->bus_specific = htonl(flags);
	/* c->addr_first/last to be filled later */
	p->vendor_id = htonll(0x46696c6544617461LL); /* "FileData" */
	p->version = htonl(1); /* FIXME: version of gensdbfs */
	/* FIXME: date */
	memset(p->name, ' ', sizeof(p->name));
	len = strlen(fname);
	if (len > sizeof(p->name)) {
		fprintf(stderr, "%s: truncating filename \"%s\"\n",
			prgname, fname);
		len = sizeof(p->name);
	}
	memcpy(p->name, fname, len);
	memcpy(&p->device_id, p->name, 4);
	p->record_type = sdb_type_device;

	return 1;
}


/* All these functions return a tree, or NULL on error, after printing msg */
static struct sdbf *scan_input(char *name, struct sdbf *parent, FILE **cfgf)
{
	DIR *d;
	struct dirent *de;
	struct sdbf *tree;
	int n, ret;

	/* first loop: count the entries */
	d = opendir(name);
	if (!d) {
		fprintf(stderr, "%s: %s: %s\n", prgname, name,
			strerror(errno));
		return NULL;
	}
	for (n = 0;  (de = readdir(d)); )
		n++;
	closedir(d);

	tree = calloc(n, sizeof(*tree));
	if (!tree) {
		fprintf(stderr, "%s: out of memory\n", prgname);
		return NULL;
	}
	tree->nfiles = n;

	/* second loop: fill it */
	d = opendir(name);
	if (!d) {
		fprintf(stderr, "%s: %s: %s\n", prgname, name,
			strerror(errno));
		return NULL;
	}
	for (n = 1 /* 0 resvd for interconnect */;  (de = readdir(d)); ) {
		tree[n].de = *de;
		if (!strcmp(de->d_name, ".")) {
			tree[0].de = *de;
			__fill_dot(tree);
			continue;
		}
		if (!strcmp(de->d_name, ".."))
			continue; /* no dot-dot */
		if (!strcmp(de->d_name, CFG_NAME)) {
			/* FIXME: cfg file */
			continue;
		}
		ret = __fill_file(tree + n, name, de->d_name);
		if (ret < 0)
			return NULL;
		n += ret;
	}
	/* number or records in the interconnect */
	tree->s_i.sdb_records = htons(n);

	return tree;
}

static int dumpstruct(FILE *dest, char *name, void *ptr, int size)
{
	int ret, i;
	unsigned char *p = ptr;

	ret = fprintf(dest, "%s (size 0x%x)\n", name, size);
	for (i = 0; i < size; ) {
		ret += fprintf(dest, "%02x", p[i]);
		i++;
		ret += fprintf(dest, i & 3 ? " " : i & 0xf ? "  " : "\n");
	}
	if (i & 0xf)
		ret += fprintf(dest, "\n");
	return ret;
}

static void dump_tree(struct sdbf *tree)
{
	int i, 	n = ntohs(tree->s_i.sdb_records);
	for (i = 0; i < n; i++, tree++) {
		printf("%s: \"%s\" ino %li\n", tree->fullname, tree->de.d_name,
		       (long)tree->de.d_ino);
		printf("astart %lx, rstart %lx, size %lx (%lx)\n",
		       tree->astart, tree->rstart, tree->size,
		       tree->stbuf.st_size);
		dumpstruct(stdout, "sdb record", &tree->s_d,
			   sizeof(tree->s_d));
		printf("\n");
	}
}

static struct sdbf *scan_config(struct sdbf *tree, FILE *f)
{
	return tree; /* FIXME: no config file yet */
}

static struct sdbf *alloc_storage(struct sdbf *tree)
{
	/* FIXME: This is just lazy, it starts at 0 and goes linear */
	int i, n;
	unsigned long pos;
	struct sdbf *f;

	n = ntohs(tree->s_i.sdb_records);
	pos = SDB_ALIGN(n * sizeof(struct sdb_device));
	tree->rstart = 0;

	for (i = 1; i < n; i++) {
		f = tree + i;
		f->rstart = pos;
		f->s_d.sdb_component.addr_first = htonll(pos);
		f->s_d.sdb_component.addr_last = htonll(pos + f->size - 1);
		pos = SDB_ALIGN(pos + f->size);
	}
	tree->s_i.sdb_component.addr_first = htonll(0);
	tree->s_i.sdb_component.addr_last = htonll(pos - 1);
	return tree;
}

static struct sdbf *write_sdb(struct sdbf *tree, FILE *out)
{
	int i, j, n, copied;
	unsigned long pos;
	struct sdbf *sdbf;
	FILE *f;
	char *buf;

	buf = malloc(blocksize);
	if (!buf) {
		fprintf(stderr, "%s: out of memory\n", prgname);
		return NULL;
	}
	n = ntohs(tree->s_i.sdb_records);
	/* First, write the directory */
	fseek(out, tree->astart, SEEK_SET);
	for (i = 0; i < n; i++)
		fwrite(&tree[i].s_d, sizeof(tree[i].s_d), 1, out);
	/* then each file */
	for (i = 1; i < n; i++) {
		sdbf = tree + i;
		f = fopen(sdbf->fullname, "r");
		if (!f) {
			fprintf(stderr, "%s: %s: %s -- ignoring\n", prgname,
				sdbf->fullname, strerror(errno));
			continue;
		}
		fseek(out, tree->astart + sdbf->rstart, SEEK_SET);
		for (copied = 0; copied < sdbf->stbuf.st_size; ) {
			j = fread(buf, 1, blocksize, f);
			if (j <= 0)
				break; /* unlikely */
			fwrite(buf, 1, j, out);
			copied += j;
			pos = ftell(out);
			if (pos > lastwritten)
				lastwritten = pos;
		}
		fclose(f);
	}
	return tree;
}


static int usage(char *prgname)
{
	fprintf(stderr, "%s: Use \"%s [<options>] <inputdir> <output>\"\n",
		prgname, prgname);
	fprintf(stderr, "  -b <number> : block size (default 64)\n");
	fprintf(stderr, "  -s <number> : device size (default: as needed)\n");
	fprintf(stderr, "  a file called \"" CFG_NAME "\", in the root of\n"
		"  <inputdir> is used as configuration file\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	struct stat stbuf;
	FILE *fcfg, *fout;
	char *rest;
	struct sdbf *tree;

	prgname = argv[0];
	while ( (c = getopt(argc, argv, "b:s:")) != -1) {
		switch (c) {
		case 'b':
			blocksize = strtol(optarg, &rest, 0);
			if (rest && *rest) {
				fprintf(stderr, "%s: not a number \"%s\"\n",
					prgname, optarg);
				exit(1);
			}
			break;
		case 's':
			devsize = strtol(optarg, &rest, 0);
			if (rest && *rest) {
				fprintf(stderr, "%s: not a number \"%s\"\n",
					prgname, optarg);
				exit(1);
			}
			break;
		}
	}
	if (optind != argc - 2)
		usage(prgname);

	/* check input and output */
	if (stat(argv[optind], &stbuf) < 0) {
		fprintf(stderr, "%s: %s: %s\n", prgname, argv[optind],
			strerror(errno));
		exit(1);
	}
	if (!S_ISDIR(stbuf.st_mode)) {
		fprintf(stderr, "%s: %s: not a directory\n", prgname,
			argv[optind]);
		exit(1);
	}
	fout = fopen(argv[optind+1], "w");
	if (!fout) {
		fprintf(stderr, "%s: %s: %s\n", prgname, argv[optind+1],
			strerror(errno));
		exit(1);
	}

	/* scan the whole input tree and save the information */
	tree = scan_input(argv[optind], NULL /* parent */, &fcfg);
	if (!tree)
		exit(1);

	/* read configuration file and save its info for each file */
	if (fcfg)
		tree = scan_config(tree, fcfg);
	if (!tree)
		exit(1);

	/* allocate space in the storage */
	tree = alloc_storage(tree);
	if (!tree)
		exit(1);

	if (getenv("VERBOSE"))
		dump_tree(tree);

	/* write out the whole tree */
	tree->astart = 0;
	tree = write_sdb(tree, fout);
	if (!tree)
		exit(1);
	if (lastwritten < devsize) {
		fseek(fout, devsize - 1, SEEK_SET);
		fwrite("\0", 1, 1, fout);
	}
	fclose(fout);
	if (devsize && (lastwritten > devsize)) {
		fprintf(stderr, "%s: data storage (0x%lx) exceeds device size"
			" (0x%lx)\n", prgname, lastwritten, devsize);
		exit(1);
	}
	exit(0);
}
