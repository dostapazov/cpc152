#Make file for  shared libraryes
TARGET  = libcpc152svc-rdopts.so.1.0.0
TARGET1 = libcpc152svc-rdopts.so.1.0
TARGET2 = libcpc152svc-rdopts.so.1
TARGET3 = libcpc152svc-rdopts.so
DBGFL= -O1

LIBVER=1.0
CC=g++
INC=../include
CFLAGS  = -pipe -fPIC -c -Wall -W -fexceptions -fshort-wchar -std=c++11 -D_REENTRANT -DRDOPTS_SHARED_LIB $(DBGFL) -I$(INC)
LIBS    = -lboost_system -L../../../lib -lgk-lib-$(LIBVER)
LDFLAGS = -shared -Wl,-soname,$(TARGET3) -o $(TARGET) -lc

SOURCES = read_options.cpp strhelper.cpp #talarms.cpp
OBJECTS = read_options.o strhelper.o #talarms.o

all: $(OBJECTS) 
	rm -f $(TARGET3) $(TARGET2) $(TARGET1) $(TARGET)
	$(CC) $(LDFLAGS) $(OBJECTS) -lc
	strip --strip-unneeded $(TARGET)
	ln -s $(TARGET) $(TARGET3)
	ln -s $(TARGET) $(TARGET2)
	ln -s $(TARGET) $(TARGET1)
#	rm -f strhelper.o talarms.o

read_options.o : read_options.cpp
	$(CC) $(CFLAGS) read_options.cpp
strhelper.o : strhelper.cpp
	$(CC) $(CFLAGS) strhelper.cpp
#talarms.o : talarms.cpp
#	$(CC) $(CFLAGS) talarms.cpp

clean:
	rm -f $(OBJECTS)
cleanall :clean
	rm -f $(TARGET3) $(TARGET2) $(TARGET1) $(TARGET)