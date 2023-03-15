
CC = gcc
CXX = g++
CFLAGS  = -O3 -Wall -pipe -g -D_GNU_SOURCE -fPIC 

OBJ= Log.o
TARGET = libWebChatLog.a
BINTARGET = Log


all: ${OBJ} ${TARGET}

$(TARGET):
	ar -r $(TARGET) ${OBJ} 

$(BINTARGET):
	g++ -o $(TARGET) ${OBJ} 
	
.c.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<
	echo $@

.cpp.o:
	$(CXX) $(CFLAGS) -c $(INCLUDE) $< 
	echo $@
%:%.c
	$(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS)
	echo $@
clean:
	rm -f $(TARGET)
	rm -f *.o
