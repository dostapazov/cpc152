DEBUG=y
USR=$(shell whoami)
PWD=$(shell pwd)
INCDIR=/home/$(USR)/cpc152/include
#INCDIR=$(PWD)../../../include
EXTRA_CFLAGS  := -I$(LDDINCDIR)
EXTRA_CFLAGS  += -I$(INCDIR)
TARGET=aic124
EXTRA_CFLAGS += -D_AIC124MOD_VER=2
MOD_PATH=/lib/modules/$(shell uname -r)/kernel/drivers/$(TARGET)

ifeq ($(DEBUG),y)
EXTRA_CFLAGS  += -O -nostdinc -D_DEBUG
else
EXTRA_CFLAGS  += -O2 -nostdinc -D_REAL_DEVICE_
endif
#CFLAGS := $(CFLAGS_NOGCOV)
ccflags-y += -I$(INCDIR) 


ifneq ($(KERNELRELEASE),)
 #call from kernel build system
obj-m := $(TARGET).o 
aic124-objs += aic124_fileop.o 
aic124-objs += aic124main.o 
aic124-objs += aic124hw.o 
aic124-objs += ../common/confparse.o 
aic124-objs += ../common/drvqueue.o 

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build #/usr/src/linux #/lib/modules/`uname -r`
PWD       := $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
#ifneq ($(DEBUG),y)
	strip --strip-debug --strip-unneeded $(TARGET).ko
#endif
endif

install:
	mkdir $(MOD_PATH)       
	cp -f $(TARGET).ko $(MOD_PATH)/$(TARGET).ko
	echo "#dostapazov@gmail.com Fastwell aic124 device" >> /etc/modules
	echo $(TARGET) >> /etc/modules
	depmod -a



clean:
	rm -rf *.o *~ core .depend .*.cmd  *.mod.c .tmp_versions *.markers  *.symvers *.order *.gcno  ../common/*.o ../common/.*.cmd ../common/.*.gcno
cleanall: 
	rm -rf *.ko
	rm -rf *.o *~ core .depend .*.cmd  *.mod.c .tmp_versions *.markers  *.symvers *.order *.gcno  ../common/*.o ../common/.*.cmd ../common/.*.gcno

 
