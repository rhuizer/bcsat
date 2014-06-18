# The path to MiniSAT version 2
#MINISAT2_PATH = /home/tommi/Public/minisat2-070721
MINISAT2_PATH = /lhome/tjunttil/Progs/minisat
# The path to ZChaff
#ZCHAFF_PATH = /home/tommi/Public/zchaff64
ZCHAFF_PATH = /lhome/tjunttil/Progs/zchaff.2007.3.12



CFLAGS = -I.
CFLAGS += -g
#CFLAGS += -pg
CFLAGS += -O3
CFLAGS += -Wall
#CFLAGS += --pedantic
#CFLAGS += -DDEBUG

OBJS = defs.o bc.o gate.o parser.tab.o lexer.o gatehash.o handle.o
OBJS += timer.o heap.o

LINK = #-static
CC = g++


#LIB += /usr/lib/ccmalloc.o -ldl #-lfl



all: bc2cnf bc2edimacs edimacs2bc bc2iscas89
all: bczchaff
all: bcminisat2core bcminisat2simp


clean:
	rm -f bc2cnf bc2cnf.o
	rm -f bc2edimacs bc2edimacs.o
	rm -f edimacs2bc edimacs2bc.o
	rm -f bc2iscas89 bc2iscas89.o
	rm -f bczchaff bczchaff.o bczchaff_solve.o
	rm -f bcminisat2core bcminisat2simp bcminisat.o bcminisat_solve.o
	rm -f $(OBJS) parser.tab.c parser.tab.h parser.output lexer.c
	rm -f *~

.cc.o:
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<

parser.tab.c parser.tab.h: parser.y
	bison -b parser -p bcp_ -d parser.y;

lexer.c: lexer.lex parser.tab.h
	flex -L -Pbcp_ -olexer.c lexer.lex;


bc2cnf: $(OBJS) bc2cnf.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) bc2cnf.o $(LIB) $(LINK) 

bc2edimacs:  defs.o bc.o gate.o gatehash.o lexer.o parser.tab.o handle.o bc2edimacs.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) bc2edimacs.o $(LIB) $(LINK) 

edimacs2bc:  defs.o bc.o gate.o gatehash.o lexer.o parser.tab.o handle.o edimacs2bc.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) edimacs2bc.o $(LIB) $(LINK) 

bc2iscas89: $(OBJS) bc2iscas89.o
	$(CC) $(CFLAGS) -o $@ $(OBJS) bc2iscas89.o $(LIB) $(LINK) 






#
# bczchaff
#
.PHONY : bczchaff
bczchaff: $(OBJS) bczchaff.o 
	$(CC) $(CFLAGS) -I$(ZCHAFF_PATH) -DBC_HAS_ZCHAFF -c bczchaff_solve.cc
	$(CC) $(CFLAGS) -o bczchaff $(OBJS) bczchaff_solve.o bczchaff.o -L$(ZCHAFF_PATH) -lsat $(LIB) $(LINK) 



#
# bcminisat
#
# MiniSat 2 core version
.PHONY : bcminisat2core
bcminisat2core: $(OBJS) bcminisat.o
	cd $(MINISAT2_PATH)/core && make lib
	$(CC) $(CFLAGS) -I$(MINISAT2_PATH) -I$(MINISAT2_PATH)/core -I$(MINISAT2_PATH)/mtl -DBC_HAS_MINISAT -DMINISAT2CORE -c bcminisat_solve.cc
	$(CC) $(CFLAGS) -o bcminisat2core $(OBJS) bcminisat_solve.o bcminisat.o $(LIB) -L$(MINISAT2_PATH)/core -lminisat $(LINK) 
# MiniSat 2 with simplifier
.PHONY : bcminisat2simp
bcminisat2simp: $(OBJS) bcminisat.o
	cd $(MINISAT2_PATH)/simp && make lib
	$(CC) $(CFLAGS) -I$(MINISAT2_PATH) -I$(MINISAT2_PATH)/core -I$(MINISAT2_PATH)/simp -I$(MINISAT2_PATH)/mtl -DBC_HAS_MINISAT -DMINISAT2SIMP -c bcminisat_solve.cc
	$(CC) $(CFLAGS) -o bcminisat2simp $(OBJS) bcminisat_solve.o bcminisat.o $(LIB) -L$(MINISAT2_PATH)/simp -lminisat $(LINK) 




# DO NOT DELETE

