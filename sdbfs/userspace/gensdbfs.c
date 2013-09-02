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
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <sdb.h>
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

static struct sdbf *prepare_dir(char *name, struct sdbf *parent);

static inline unsigned long SDB_ALIGN(unsigned long x)
{
	return (x + (blocksize - 1)) & ~(blocksize - 1);
}

static void __fill_product(struct sdb_product *p, char *name, time_t t,
			   int record_type)
{
	int len = strlen(name);

	if (len > sizeof(p->name)) {
		fprintf(stderr, "%s: truncating filename \"%s\"\n",
			prgname, name);
		len = sizeof(p->name);
	}
	memset(p->name, ' ', sizeof(p->name));
	memcpy(p->name, name, len);
	memcpy(&p->device_id, p->name, sizeof(p->device_id));
	p->vendor_id = DEFAULT_VENDOR; /* changed by config, possibly */
	p->version = htonl(1); /* FIXME: version of gensdbfs */
	/* FIXME: date */
	p->record_type = record_type;
}

/* Helpers for scan_inputdir(), which is below */
static void __fill_dot(struct sdbf *dot, char *dir)
{
	struct sdb_interconnect *i = &dot->s_i;
	struct sdb_component *c = &i->sdb_component;
	struct sdb_product *p = &c->product;
	char fn[PATH_MAX];

	strcpy(fn, dir);
	strcat(fn, "/.");
	dot->fullname = strdup(fn);
	dot->basename = strdup(".");
	i->sdb_magic = htonl(SDB_MAGIC);
	i->sdb_version = 1;
	i->sdb_bus_type = sdb_data;
	/* c->addr_first/last to be filled later */
	__fill_product(p, ".", 0 /* date */, sdb_type_interconnect);
}

/* Helper for __fill_file, below, f->stat and f->fullname already valid */
static int __fill_dir(struct sdbf *f)
{
	struct sdb_bridge *b = &f->s_b;
	struct sdb_component *c = &b->sdb_component;
	struct sdb_product *p = &c->product;

	/* addr first and last filled later */
	__fill_product(p, f->basename, f->stbuf.st_mtime, sdb_type_bridge);
	f->subdir = prepare_dir(f->fullname, f->dot);
	if (!f->subdir)
		return -1;
	return 1;
}

static int __fill_file(struct sdbf *f, char *dir, char *fname)
{
	char fn[PATH_MAX];
	struct sdb_device *d = &f->s_d;
	struct sdb_component *c = &d->sdb_component;
	struct sdb_product *p = &c->product;
	int flags;

	strcpy(fn, dir);
	strcat(fn, "/");
	strcat(fn, fname);
	f->fullname = strdup(fn);
	f->basename = strdup(fname);
	if (stat(fn, &f->stbuf) < 0) {
		fprintf(stderr, "%s: stat(%s): %s\n", prgname, fn,
			strerror(errno));
		return -1;
	}
	if (S_ISDIR(f->stbuf.st_mode))
		return __fill_dir(f);
	if (!S_ISREG(f->stbuf.st_mode)) {
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
	__fill_product(p, f->basename, f->stbuf.st_mtime, sdb_type_device);
	return 1;
}

/* Helpers for scan_config(), which is below */
static struct sdbf *find_filename(struct sdbf *tree, char *s)
{
	int i, n = ntohs(tree->s_i.sdb_records);
	struct sdbf *f;

	for (i = 0; i < n; i++) {
		f = tree + i;
		if (!strcmp(s, f->basename))
			return f;
	}
	return NULL;
}

static int parse_config_line(struct sdbf *tree, struct sdbf *current, int line,
			      char *t)
{
	struct sdb_device *d = &current->s_d;
	struct sdb_component *c = &d->sdb_component;
	struct sdb_product *p = &c->product;
	unsigned long int32; /* may be 64 bits on some machines */
	unsigned long long int64;
	int i;

	if (getenv("VERBOSE"))
		fprintf(stderr, "parse line %i for %s: %s\n", line,
			current->fullname, t);

	/*
	 * Unfortunately, scanning as %i refuses "negative" hex values,
	 * saturating at 0x7fffffff. But %u refuses the leading 0x.
	 * In order to accept both positive decimal and hex, use %u first,
	 * and if it returns 0 use %x. I still think hex saturation is a bug.
	 */
	if (sscanf(t, "vendor = %llu", &int64) == 1) {
		if (int64 == 0)
			sscanf(t, "vendor = %llx", &int64);
		p->vendor_id = htonll(int64);
		return 0;
	}
	if (sscanf(t, "device = %lu", &int32) == 1) {
		if (int32 == 0)
			sscanf(t, "device = %lx", &int32);
		p->device_id = htonl(int32);
		return 0;
	}
	if (sscanf(t, "write = %i", &i) == 1) {
		if (i)
			d->bus_specific |= htonl(SDB_DATA_WRITE);
		else
			d->bus_specific &= htonl(~SDB_DATA_WRITE);
		return 0;
	}
	if (sscanf(t, "maxsize = %li", &int32) == 1) {
		current->size = int32;
		return 0;
	}
	if (sscanf(t, "position = %li", &int32) == 1) {
		if (tree->level != 0) {
			fprintf(stderr, "%s: Can't set position in subdirs"
			       " (file \"%s\")\n", prgname, current->fullname);
			return 0;
		}
		current->userpos = 1;
		current->ustart = int32;
		return 0;
	}

	fprintf(stderr, "%s: %s:%i: Unknown directive \"%s\" for file \"%s\"\n",
		prgname, CFG_NAME, line, t, current->fullname);
	return -1;
}

/* step 0: read the directory and build the tree. Returns NULL on error */
static struct sdbf *scan_inputdir(char *name, struct sdbf *parent, FILE **cfgf)
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
	tree->nfiles = n; /* FIXME: increase this nfile according to cfg */
	if (parent)
		tree->level = parent->level + 1;

	/* second loop: fill it */
	d = opendir(name);
	if (!d) {
		fprintf(stderr, "%s: %s: %s\n", prgname, name,
			strerror(errno));
		return NULL;
	}
	for (n = 1;  (de = readdir(d)); ) {
		/* dot is special: it fills slot 0 */
		if (!strcmp(de->d_name, ".")) {
			tree[0].de = *de;
			__fill_dot(tree, name);
			continue;
		}
		if (!strcmp(de->d_name, ".."))
			continue; /* no dot-dot */
		if (!strcmp(de->d_name, CFG_NAME)) {
			char s[PATH_MAX];

			strcpy(s, name);
			strcat(s, "/");
			strcat(s, de->d_name);
			*cfgf = fopen(s, "r");
			if (!*cfgf)
				fprintf(stderr, "%s: open(%s): %s\n",
					prgname, CFG_NAME, strerror(errno));
			/* don't exit on this error: proceed without cfg */
			continue;
		}
		tree[n].level = tree[0].level;
		tree[n].de = *de;
		tree[n].dot = tree;
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
		printf("ustart %lx, rstart %lx, base %lx, size %lx (%lx)\n",
		       tree->ustart, tree->rstart, tree->base, tree->size,
		       tree->stbuf.st_size);
		dumpstruct(stdout, "sdb record", &tree->s_d,
			   sizeof(tree->s_d));
		printf("\n");
	}
}

/* step 1: change the in-memory tree according to config file */
static struct sdbf *scan_config(struct sdbf *tree, FILE *f)
{
	struct sdbf *current = NULL;
	char s[256];
	char *t;
	int i, lineno = 0;

	while (fgets(s, sizeof(s), f)) {
		lineno++;
		for (i = strlen(s) - 1; i >= 0 && isspace(s[i]); i--)
			s[i] = '\0';
		t = s;
		while (*t && isblank(*t))
			t++;
		if (*t == '#' || !*t) /* empty or comment */
			continue;
		if (t == s) {
			/* line starts in column 0: new file name */
			current = find_filename(tree, s);
			if (!current) {
				/* FIXME: possibly increase nfile here */
				fprintf(stderr, "%s: Warning: %s:%i: "
					"\"%s\" not found\n",
					prgname, CFG_NAME, lineno, s);
			}
			continue;
		}
		if (!current) {
			/* ignore directives for non-existent files */
			continue;
		}
		parse_config_line(tree, current, lineno, t);
	}
	return tree;
}

/* step 2: place the files in the storage area */
static struct sdbf *alloc_storage(struct sdbf *tree)
{
	int i, n;
	unsigned long subsize;
	unsigned long rpos; /* the next expected relative position */
	unsigned long l, last; /* keep track of last, for directory record */
	struct sdbf *f, *sub;

	/* The managed space starts at zero, even if the directory is later */
	tree->s_i.sdb_component.addr_first = htonll(0);
	/* The "suggested" output place is after the directory itself */
	n = ntohs(tree->s_i.sdb_records);
	rpos = tree->ustart + SDB_ALIGN(n * sizeof(struct sdb_device));
	last = rpos;

	for (i = 1; i < n; i++) {
		f = tree + i;

		/* If a directory, make it allocate itself */
		if (f->subdir) {
			f->subdir->base = tree->base + rpos;
			sub = alloc_storage(f->subdir);
			if (!sub) {
				fprintf(stderr, "%s: Error allocating %s\n",
					prgname, f->fullname);
				return NULL;
			}
			/* this size may have been set by the user */
			subsize = ntohll(sub->s_i.sdb_component.addr_last);
			if (subsize > f->size)
				f->size = subsize;
			f->s_b.sdb_child = htonll(rpos);
		}

		if (f->userpos) { /* user-specified position (level 0) */
			f->s_d.sdb_component.addr_first = htonll(f->ustart);
			l = f->ustart + f->size - 1;
			f->s_d.sdb_component.addr_last = htonll(l);
			if (l > last) last = l;
			continue;
		}

		/* position not mandated: go sequential from previous one */
		f->rstart = rpos;
		f->s_d.sdb_component.addr_first = htonll(rpos);
		l = rpos + f->size - 1;
		f->s_d.sdb_component.addr_last = htonll(l);
		if (l > last) last = l;
		if (getenv("VERBOSE"))
			fprintf(stderr, "allocated relative %s: %lx to %lx\n",
				f->fullname, rpos, l);
		rpos = SDB_ALIGN(rpos + f->size);
	}
	/* finally, save the last used byte for the whole directory */
	tree->s_i.sdb_component.addr_last = htonll(last);
	return tree;
}

/* step 3: output the image file */
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

	/*
	 * First, write the directory, from its possibly user-set position.
	 * Meanwhile, update base for each of them (used in subdirs)
	 */
	fseek(out, tree->base + tree->ustart, SEEK_SET);
	for (i = 0; i < n; i++) {
		fwrite(&tree[i].s_d, sizeof(tree[i].s_d), 1, out);
		if (i > 1) /* don't change initial base */
			tree[i].base = tree[0].base + tree[i].rstart;
	}
	if (getenv("VERBOSE")) /* show the user */
		dump_tree(tree);

	/* then each file */
	for (i = 1; i < n; i++) {
		sdbf = tree + i;
		f = fopen(sdbf->fullname, "r");
		if (!f) {
			fprintf(stderr, "%s: %s: %s -- ignoring\n", prgname,
				sdbf->fullname, strerror(errno));
			continue;
		}

		if (sdbf->userpos) /* only at level 0 */
			fseek(out, sdbf->ustart, SEEK_SET);
		else
			fseek(out, tree->base + sdbf->rstart, SEEK_SET);

		if (sdbf->subdir) {
			write_sdb(sdbf->subdir, out);
			fclose(f);
			continue;
		}

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
	free(buf);
	return tree;
}

/*
 * This is the main procedure for each directory, called recursively
 * from scan_inputdir() above
 */
static struct sdbf *prepare_dir(char *name, struct sdbf *parent)
{
	FILE *fcfg = NULL;
	struct sdbf *tree;

	/* scan the whole input tree and save the information */
	tree = scan_inputdir(name, parent, &fcfg);
	if (!tree)
		return NULL;

	/* read configuration file and save its info for each file */
	if (fcfg) {
		tree = scan_config(tree, fcfg);
		fclose(fcfg);
	}
	if (!tree)
		return NULL;

	return tree;
}

/* From now on, it's trivial main program management */
static int usage(char *prgname)
{
	fprintf(stderr, "%s: Use \"%s [<options>] <inputdir> <output>\"\n",
		prgname, prgname);
	fprintf(stderr, "  -b <number> : block size (default 64)\n");
	fprintf(stderr, "  -s <number> : device size (default: as needed)\n");
	fprintf(stderr, "  a file called \"" CFG_NAME "\", in each "
		"subdir is used as configuration file\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	struct stat stbuf;
	FILE *fout;
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
		/* Recursively fill the directory */
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

	tree = prepare_dir(argv[optind], NULL /* parent */);
	if (!tree)
		exit(1);

	/* allocate space in the storage */
	tree = alloc_storage(tree);
	if (!tree)
		exit(1);

	/* write out the whole tree, recusively */
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
