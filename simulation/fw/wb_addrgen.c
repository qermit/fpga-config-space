/*
 * Wishbone memory map firmware generator
 *
 * Author: Manohar Vanga <manohar.vanga@cern.ch>
 * Copyright (C) 2011 CERN
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <linux/sdwb.h>

#define PRIORITY(flag) 	((flag >> 28) & 0xf)
#define CLASS(flag)		((flag >> 16) & 0xfff)
#define VERSION(flag)	(flag & 0xffff)

void print_wbd(uint8_t major, uint8_t minor,
				uint64_t vendor, uint32_t device,
				uint64_t base, uint64_t size,
				uint32_t flags, uint32_t class,
				uint32_t version, uint32_t date,
				char *vname, char *dname)
{
		printf("Wishbone Device:\n");
		printf("\tMajor: %d, Minor: %d\n", major, minor);
		printf("\tVendor: 0x%llx, Device: 0x%x\n", vendor, device);
		printf("\tBase: 0x%llx, Size: 0x%llx\n", base, size);
		printf("\tFlags: 0x%x, Class: 0x%x\n", flags, class);
		printf("\tVers: %d, Date: 0x%x\n", version, date);
		printf("\tVName: %s\n\tDName: %s\n", vname, dname);
}

struct sdwb_head *sdwb_create_header(uint64_t wbid_addr, uint64_t wbd_addr)
{
	struct sdwb_head *head;

	head = malloc(sizeof(struct sdwb_head));
	if (!head)
		return NULL;
	
	head->magic = SDWB_HEAD_MAGIC;
	head->wbid_address = wbid_addr;
	head->wbd_address = wbd_addr;

	return head;
}

struct sdwb_wbid *sdwb_create_id()
{
	struct sdwb_wbid *id;

	id = malloc(sizeof(struct sdwb_wbid));
	if (!id)
		return NULL;
	
	id->bstream_type = rand();

	return id;
}

struct sdwb_wbd *sdwb_create_device(uint8_t major, uint8_t minor,
						uint64_t vendor, uint32_t device,
						uint64_t base, uint64_t size,
						uint32_t flags, uint32_t class,
						uint32_t version, uint32_t date,
						char *vname, char *dname)
{
	struct sdwb_wbd *dev;

	dev = malloc(sizeof(struct sdwb_wbd));
	if (!dev)
		return NULL;

	dev->wbd_magic = SDWB_WBD_MAGIC;
	dev->wbd_version = (((major & 0xFF) << 8) | ((minor) & 0xFF));
	dev->vendor = vendor;
	dev->device = device;
	dev->hdl_base = base;
	dev->hdl_size = size;
	dev->wbd_flags = flags;
	dev->hdl_class = class;
	dev->hdl_version = version;
	dev->hdl_date = date;
	strncpy(dev->vendor_name, vname, 16);
	strncpy(dev->device_name, dname, 16);
	dev->vendor_name[15] = '\0';
	dev->device_name[15] = '\0';

	return dev;
}

int main(int argc, char *argv[])
{
	int i;
	char c;
	struct sdwb_head *header;
	struct sdwb_wbid *id;

	if (argc != 3) {
		printf("usage: %s <device-list-file> <fw-file>\n", argv[0]);
		return 1;
	}
	FILE *fin, *fout;
	char buf[128];

	/* Header placed at address 0 and ID immediately after it. followed by devices */
	header = sdwb_create_header(sizeof(struct sdwb_head), sizeof(struct sdwb_head) + sizeof(struct sdwb_wbid));
	id = sdwb_create_id();

	fin = fopen(argv[1], "r");
	if (!fin)
		return 1;
	fout = fopen(argv[2], "w");
	if (!fout) {
		fclose(fin);
		return 1;
	}
	int size = 0;
	fwrite(header, sizeof(struct sdwb_head), 1, fout);
	size += sizeof(struct sdwb_head);
	fwrite(id, sizeof(struct sdwb_wbid), 1, fout);
	size += sizeof(struct sdwb_wbid);
	int num = 0;
	while (fgets(buf, 128, fin) != NULL) {
		if (strlen(buf) == 0)
			continue;
		if (buf[0] == '#' || buf[0] == '\n')
			continue;
		uint32_t major, minor, vendor, base, sz;
		uint32_t device, flags, class, version, date;
		char vname[16], dname[16];
		sscanf(buf, "%d %d %x %d %x %x %x %x %d %x\n", &major, &minor, &vendor, &device,
						&base, &sz, &flags, &class, &version, &date);
		fgets(buf, 128, fin);
		sscanf(buf, "%s\n", vname);
		fgets(buf, 128, fin);
		sscanf(buf, "%s\n", dname);

		print_wbd(major, minor, vendor, device, base, sz, flags, class, version, date, vname, dname);
		struct sdwb_wbd *dev;
		dev = sdwb_create_device(major, minor, vendor, device, base, sz, flags, class, version, date, vname, dname);
		fwrite(dev, sizeof(struct sdwb_wbd), 1, fout);
		size += sizeof(struct sdwb_wbd);
		num++;
	}

	i = 4*1024*1024 - size;
	printf("fill: 0x%x bytes\n", i);
	c = 0;
	while (--i >= 0)
		fwrite(&c, 1, 1, fout);
	fclose(fin);
	fclose(fout);

	return 0;
}
