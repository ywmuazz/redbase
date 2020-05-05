.PHONY:clean
CC=g++
BUILD_DIR=./build/
INC_DIRS=-I.
CFLAGS=-g -O1 -Wall $(INC_DIRS)
BIN=redbase
OBJS=main.o pf_manager.o pf_pagehandle.o pf_filehandle.o \
pf_hashtable.o pf_error.o pf_buffermgr.o 

$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o:%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(BIN)
