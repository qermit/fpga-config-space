#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define PRIORITY(flag) 	((flag >> 28) & 0xf)
#define CLASS(flag)		((flag >> 16) & 0xfff)
#define VERSION(flag)	(flag & 0xffff)

struct wb_id {
	uint32_t vendor;
	uint16_t device;
	uint16_t subdevice;
	uint32_t flags;
} __attribute__((aligned(1024)));

int main(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s <device-list-file> <fw-file>\n", argv[0]);
		return 1;
	}
	FILE *fin, *fout;
	char buf[128];
	fin = fopen(argv[1], "r");
	if (!fin)
		return 1;
	fout = fopen(argv[2], "w");
	if (!fout) {
		fclose(fin);
		return 1;
	}
	struct wb_id id;
	int num = 0;
	while (fgets(buf, 128, fin) != NULL) {
		if (buf[0] == '#')
			continue;
		int priority, class, version;
		sscanf(buf, "%d %d %d %d %d %d\n", &id.vendor, &id.device,
		         &id.subdevice, &priority, &class, &version);
		printf("Device:\n\tVendor: %d\n\tDevice: %d\n\tSubdevice: %d\n\tPriority: %d\n\tClass: %d\n\tVersion: %d\n", id.vendor, id.device, id.subdevice, priority & 0xf, class & 0xfff, version & 0xffff);
		id.flags = ((priority & 0xf) << 28) | ((class & 0xfff) << 16) | (version & 0xffff);
		fwrite((void *)&id, sizeof(struct wb_id), 1, fout);
		num++;
	}
	memset(&id, 0, sizeof(struct wb_id));
	while (num++ < 4096)
		fwrite((void *)&id, sizeof(struct wb_id), 1, fout);
	fclose(fin);
	fclose(fout);
	return 0;
}
