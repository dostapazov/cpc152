#
# Makefile for internal Vortex DX watchdog
#

ifneq ($KERNELRELEASE),)

obj-m		:= vxdx_wdt0.o vxdx_wdt1.o

EXTRA_CFLAGS 	+= -D__LINUX__
CC    		:= gcc

else

KDIR		:= /usr/src/linux
PWD		:= $(shell pwd)

default:
    $(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
endif
