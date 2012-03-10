KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)
ENTRY_ADDR="\#define ENTRY_ADDR 0x`grep handle_mm_fault /boot/System.map-\`uname -r\` | awk '{ print $$1; }'`"
EXIT_ADDR="\#define EXIT_ADDR 0x`grep lru_cache_add_active /boot/System.map-\`uname -r\` | awk '{ print $$1; }'`"

obj-m := pfdura-mod.o

default:
	@echo $(ENTRY_ADDR) > config.h
	@echo $(EXIT_ADDR) >> config.h
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(CC) -o pfdura pfdura.c -lpthread

clean:
	/bin/rm -rf *.o *.ko *~ core *.mod.c .*.cmd .tmp_versions pfdura config.h module.order Module.*