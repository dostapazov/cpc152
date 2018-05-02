LIBVER=1.0
CC = g++
INC = ../../../include/
BUILD_DEF=-D_REENTRANT 
#BUILD_DEF+=-DALARM_WRITE_TWO_STEP
CFLAGS = -pipe -fPIC -c -Wall -fshort-wchar -O1 -fexceptions -std=c++11 $(BUILD_DEF) -I$(INC)  
#CFLAGS=-c -Wall  -O1 -I$(INC) -std=c++11

#LDFLAGS= -lpthread -lboost_system -lboost_chrono -lboost_thread -lboost_filesystem  -L../../../lib -lgk-lib-$(LIBVER)
LIBS = -lpthread -lboost_system -lboost_chrono -lboost_thread -lboost_filesystem -ldl  -L../../../lib -lgk-lib-$(LIBVER)
LDFLAGS = 

EXECUTABLE=cpc152svc
EXECUTABLE_CONFIG=$(EXECUTABLE).conf

DAEMON=/usr/sbin/$(EXECUTABLE)
DAEMON_CONFIG=/etc/$(EXECUTABLE_CONFIG)
DAEMON_START=/etc/init.d/$(EXECUTABLE)

SOURCES = talarms.cpp strhelper.cpp main.cpp application.cpp  event.cpp  time_util.cpp otd_data.cpp dev_poller.cpp server_imp.cpp  tfast_queue.cpp cpc152_raw_imp.cpp

OBJECTS = talarms.o   strhelper.o   main.o   application.o    event.o    time_util.o   otd_data.o   dev_poller.o   server_imp.o   tfast_queue.o   cpc152_raw_imp.o


cpc152svc: $(OBJECTS) $(SOURCES) 
	$(CC) -o $(EXECUTABLE)  $(OBJECTS) $(LDFLAGS) $(LIBS)
#	strip --strip-unneeded $(EXECUTABLE)

OBJECTS:  talarms.o strhelper.o main.o application.o  event.o time_util.o otd_data.o dev_poller.o server_imp.o  tfast_queue.o pch.h.gch  cpc152_raw_imp.o

pch.h.gch: pch.h $(INC)/*.h* 
	$(CC) $(CFLAGS) pch.h

talarms.o :talarms.cpp pch.h.gch
	$(CC) $(CFLAGS)  talarms.cpp 
strhelper.o :strhelper.cpp pch.h.gch
	$(CC) $(CFLAGS)  strhelper.cpp 
server_imp.o :server_imp.cpp pch.h.gch
	$(CC) $(CFLAGS) server_imp.cpp 
dev_poller.o: dev_poller.cpp  pch.h.gch
	$(CC) $(CFLAGS) dev_poller.cpp
otd_data.o: otd_data.cpp pch.h.gch 
	$(CC) $(CFLAGS)  otd_data.cpp
time_util.o: time_util.cpp pch.h.gch 
	$(CC) $(CFLAGS) time_util.cpp
event.o: event.cpp pch.h.gch 
	$(CC) $(CFLAGS) event.cpp
application.o: application.cpp pch.h.gch 
	$(CC) $(CFLAGS) application.cpp
main.o: main.cpp pch.h.gch 
	$(CC) $(CFLAGS) main.cpp
tfast_queue.o: tfast_queue.cpp pch.h.gch 
	$(CC) $(CFLAGS) tfast_queue.cpp
cpc152_raw_imp.o: cpc152_raw_imp.cpp pch.h.gch 
	$(CC) $(CFLAGS) cpc152_raw_imp.cpp

clean:
	-rm -f *.o *.gch

cleanall:
	-rm -f $(EXECUTABLE) *.o *.gch
	
install:
	echo "copy cpc152svc to /usr/sbin"
	service $(EXECUTABLE) stop
	cp --remove-destination $(EXECUTABLE) $(DAEMON)
	cp --remove-destination $(EXECUTABLE).sh  $(DAEMON_START)
	cp --remove-destination $(EXECUTABLE).conf $(DAEMON_CONFIG)
	update-rc.d $(EXECUTABLE) defaults
start:install
	service $(EXECUTABLE) start 
	

