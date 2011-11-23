#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include "../wb_onewire.h"

int main(int argc, char *argv[])
{
	int fd;
	int ret;
	FILE *file;
	int len;
	char *buffer;
	int num;
	char devname[20];
	struct wb_onewire_arg a;

	if (argc != 2) {
		printf("usage: %s <num>\n", argv[0]);
		exit(1);
	}

	a.num = atoi(argv[1]);
	sprintf(devname, "/dev/onewire");

	fd = open(devname, O_RDWR);
	if (fd < 0)
		return fd;

	if ((ret = ioctl(fd, READ_TEMP, &a)) < 0) {
		printf("Error: %d\n", ret);
		return ret;
	}

	printf("Temp: %08x\n", a.temp/16.0);

	close(fd);
	return 0;
}

