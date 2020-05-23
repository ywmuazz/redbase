.PHONY:clean
CC=g++
BUILD_DIR=./build/
INC_DIRS=-I.
CFLAGS=-g -O0 -Wall $(INC_DIRS)
BIN=redbase
OBJS=ix_test.o pf_manager.o pf_pagehandle.o pf_filehandle.o \
	pf_hashtable.o pf_error.o pf_buffermgr.o \
	rm_rid.o rm_record.o rm_manager.o rm_filescan.o rm_filehandle.o rm_error.o \
	ix_manager.o ix_indexscan.o ix_indexhandle.o ix_error.o


$(BIN):$(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o:%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(BIN)
