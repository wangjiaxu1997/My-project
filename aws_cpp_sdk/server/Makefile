
CC = g++ 

LOG_INC = -I../../log
LOG_LIB = -L../../log -lWebChatLog

PLUGIN_INC = -I../plugin

CFLAGS = -Wall -g -pipe -static-libstdc++ -D_NEW_LIC -D_GNU_SOURCE -fPIC -D_REENTRANT -DUSE_EPOLL -I. ${PLUGIN_INC} ${LOG_INC}

LDFLAGS =  $(CFLAGS) $(LIBDIR) $(LIB) $(LOG_LIB)
LIB = -ldl -rdynamic 
OO = Options.o Common.o aws_test.o

BIN = M_Aws_Svr

all:$(OO)
	$(CC) $(OO) $(LDFLAGS)  -o $(BIN)
	rm *.o
	mv $(BIN) /home/jiaxu/code/aws_cpp/bin

.cpp.o:
	$(CC)  $(CFLAGS) -c  $(INCLUDE) $< 
	echo $@
	
%:%.cpp 
	$(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS) 
	echo $@

clean:
	rm -f *.o
	rm -f $(BIN)



