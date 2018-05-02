LIBVER=1.0
CC=g++
INC=../../../../include/


CFLAGS=-c -Wall -O3 -I$(INC) -std=c++11
LDFLAGS= -lpthread  -lboost_system #-L../../../../lib -lgk-lib-$(LIBVER)

EXECUTABLE1=aic124test
EXECUTABLE2=dic120test
OBJECTS =  aic124test.o integr_data.o dic120test.o

all: aic124test dic120test
	
	
aic124test: aic124test.o integr_data.o
	$(CC) -o $(EXECUTABLE1) aic124test.o integr_data.o $(LDFLAGS)
	
dic120test: dic120test.o 
	$(CC) -o $(EXECUTABLE2) dic120test.o $(LDFLAGS)
	
OBJECTS :  aic124test.o integr_data.o dic120test.o

integr_data.o : $(INC)/*.h integr_data.cpp
	$(CC) $(CFLAGS) integr_data.cpp 

aic124test.o : $(INC)/*.h aic124test.cpp
	$(CC) $(CFLAGS) aic124test.cpp 
	
dic120test.o : $(INC)/*.h dic120test.cpp
	$(CC) $(CFLAGS) dic120test.cpp 

         
clean:
	-rm -f *.o
cleanall:
	-rm -f $(EXECUTABLE1) $(EXECUTABLE2) *.o

