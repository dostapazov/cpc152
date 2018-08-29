LIBVER=1.0

LIBNAME=libgk-lib-$(LIBVER).a 

CC=gcc
RDIR=../..
INC=$(RDIR)/include
LDIR=$(RDIR)/lib
BDIR=$(RDIR)/build

CFLAGS=-c -fshort-wchar -Wall -O2  -I$(INC)



objs=    modbus_proto.o \
         otd_addr.o \
         otd_base.o \
         otd_proto.o \
         otd_arch_proto.o \
         otd_proto_medium.o \
         otd_text.o \
         secur_coder.o \
         secur_proto.o \
         rx_buffer.o \
         mproto_buffer.o \
         otd_storage.o \
         calc_kpk.o
         
         
objects= $(objs)

libar:   $(objects)
	ar -r -c -s $(LIBNAME)  $(objects)
	

objects :$(objs) 


modbus_proto.o : modbus_proto.cpp
	$(CC) $(CFLAGS) modbus_proto.cpp
otd_addr.o : otd_addr.cpp
	$(CC) $(CFLAGS) otd_addr.cpp
otd_base.o : otd_base.cpp
	$(CC) $(CFLAGS) otd_base.cpp
otd_proto.o : otd_proto.cpp
	$(CC) $(CFLAGS) otd_proto.cpp
otd_arch_proto.o : otd_arch_proto.cpp
	$(CC) $(CFLAGS) otd_arch_proto.cpp
otd_proto_medium.o : otd_proto_medium.cpp
	$(CC) $(CFLAGS) otd_proto_medium.cpp
otd_text.o : otd_text.cpp
	$(CC) $(CFLAGS) otd_text.cpp
secur_coder.o : secur_coder.cpp
	$(CC) $(CFLAGS) secur_coder.cpp
secur_proto.o : secur_proto.cpp
	$(CC) $(CFLAGS) secur_proto.cpp
rx_buffer.o : rx_buffer.cpp
	$(CC) $(CFLAGS) rx_buffer.cpp
mproto_buffer.o : mproto_buffer.cpp
	$(CC) $(CFLAGS) mproto_buffer.cpp
otd_storage.o : otd_storage.cpp
	$(CC) $(CFLAGS) otd_storage.cpp
calc_kpk.o    :calc_kpk.cpp
	$(CC) $(CFLAGS) calc_kpk.cpp
install : libar
	cp --remove-destination $(LIBNAME) $(LDIR)/$(LIBNAME)
       
	
.PHONY : clean cleanall
clean  :
	echo remove all obj 
	-rm -f $(objects) $(LIBNAME)
cleanall: clean	
	-rm  $(LDIR)/$(LIBNAME)





