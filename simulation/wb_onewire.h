#ifndef WB_ONEWIRE_H
#define WB_ONEWIRE_H

struct wb_onewire_arg {
	int num;
	int temp;
};

#define __WB_ONEWIRE_IOC_MAGIC 'S'

#define READ_TEMP _IOWR(__WB_ONEWIRE_IOC_MAGIC, 0, struct wb_onewire_arg *)

#endif
