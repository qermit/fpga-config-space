
DIRS = kernel lib userspace doc

all clean modules install modules_install:
	for d in $(DIRS); do $(MAKE) -C $$d $@ || exit 1; done
