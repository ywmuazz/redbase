.PHONY:clean
CC=g++
YACC=bison -dy
LEX=flex
AR=ar -rc
RANLIB=ranlib
BUILD_DIR=./build/
LIB_DIR=./lib/
INC_DIRS=-I.

CFLAGS= -O3 -Wall $(INC_DIRS)
# OBJS=dbdestroy.o dbcreate.o redbase.o\
# 	pf_manager.o pf_pagehandle.o pf_filehandle.o \
# 	pf_hashtable.o pf_error.o pf_buffermgr.o \
# 	rm_rid.o rm_record.o rm_manager.o rm_filescan.o rm_filehandle.o rm_error.o \
# 	ix_manager.o ix_indexscan.o ix_indexhandle.o ix_error.o \
# 	sm_attriterator.o sm_error.o sm_manager.o statistics.o



PF_SOURCES     = pf_buffermgr.cpp pf_error.cpp pf_filehandle.cpp \
                 pf_pagehandle.cpp pf_hashtable.cpp pf_manager.cpp \
                 statistics.cpp #  pf_statistics.cc 

RM_SOURCES     = rm_manager.cpp rm_filehandle.cpp rm_record.cpp \
                 rm_filescan.cpp rm_error.cpp rm_rid.cpp

IX_SOURCES     = ix_manager.cpp ix_indexhandle.cpp ix_indexscan.cpp ix_error.cpp

SM_SOURCES     = sm_manager.cpp printer.cpp sm_error.cpp sm_attriterator.cpp

QL_SOURCES     = ql_error.cpp ql_manager.cpp ql_node.cpp ql_nodeproj.cpp ql_noderel.cpp ql_nodejoin.cpp ql_nodesel.cpp

UTILS_SOURCES  = dbcreate.cpp dbdestroy.cpp redbase.cpp tester.cpp

PARSER_SOURCES = scan.c parse.c nodes.c interp.cpp



PF_OBJECTS     = $(PF_SOURCES:.cpp=.o)
RM_OBJECTS     = $(RM_SOURCES:.cpp=.o)
IX_OBJECTS     = $(IX_SOURCES:.cpp=.o)
SM_OBJECTS     = $(SM_SOURCES:.cpp=.o)
QL_OBJECTS     = $(QL_SOURCES:.cpp=.o)
UTILS_OBJECTS  = $(UTILS_SOURCES:.cpp=.o)
PARSER_OBJECTS = scan.o parse.o nodes.o interp.o
# TESTER_OBJECTS =$(TESTER_SOURCES:.cc=.o)
OBJS        = $(PF_OBJECTS) $(RM_OBJECTS) $(IX_OBJECTS) \
                $(SM_OBJECTS) $(QL_OBJECTS) $(UTILS_OBJECTS)
UTILS= $(UTILS_SOURCES:.cpp=)

LIBRARY_PF     = $(LIB_DIR)libpf.a
LIBRARY_RM     = $(LIB_DIR)librm.a
LIBRARY_IX     = $(LIB_DIR)libix.a
LIBRARY_SM     = $(LIB_DIR)libsm.a
LIBRARY_QL     = $(LIB_DIR)libql.a
LIBRARY_PARSER = $(LIB_DIR)libparser.a
LIBRARIES      = $(LIBRARY_PF) $(LIBRARY_RM) $(LIBRARY_IX) \
                 $(LIBRARY_SM) $(LIBRARY_QL) $(LIBRARY_PARSER)
LIBS           = -lparser -lql -lsm -lix -lrm -lpf

#all
all: $(LIBRARIES) $(UTILS)


$(LIBRARY_PF): $(PF_OBJECTS)
	$(AR) $(LIBRARY_PF) $(PF_OBJECTS)
	$(RANLIB) $(LIBRARY_PF)

$(LIBRARY_RM): $(RM_OBJECTS)
	$(AR) $(LIBRARY_RM) $(RM_OBJECTS)
	$(RANLIB) $(LIBRARY_RM)

$(LIBRARY_IX): $(IX_OBJECTS)
	$(AR) $(LIBRARY_IX) $(IX_OBJECTS)
	$(RANLIB) $(LIBRARY_IX)

$(LIBRARY_SM): $(SM_OBJECTS)
	$(AR) $(LIBRARY_SM) $(SM_OBJECTS)
	$(RANLIB) $(LIBRARY_SM)

$(LIBRARY_QL): $(QL_OBJECTS)
	$(AR) $(LIBRARY_QL) $(QL_OBJECTS)
	$(RANLIB) $(LIBRARY_QL)

$(LIBRARY_PARSER): $(PARSER_OBJECTS)
	$(AR) $(LIBRARY_PARSER) $(PARSER_OBJECTS)
	$(RANLIB) $(LIBRARY_PARSER)





# rules

# dbcreate: dbcreate.o
# 	$(CC) $(CFLAGS) -c $< -o $@
# dbdestroy: dbdestroy.o
# 	$(CC) $(CFLAGS) -c $< -o $@
# redbase: redbase.o
# 	$(CC) $(CFLAGS) -c $< -o $@

# Parser
y.tab.h: parse.c

parse.c: parse.y
	$(YACC) parse.y; mv y.tab.c parse.c

scan.c: scan.l scanhelp.c y.tab.h
	$(LEX) scan.l; mv lex.yy.c scan.c

parse.o: parse.c

scan.o: scan.c y.tab.h

nodes.o: nodes.c

interp.o: interp.cpp


clean:
	rm -f *.o $(UTILS) y.tab.h parse.c scan.c $(LIBRARIES)

$(OBJS): %.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(UTILS): %: %.o $(LIBRARIES)
	$(CC) $(CFLAGS) $< -o $@ -L$(LIB_DIR) $(LIBS)