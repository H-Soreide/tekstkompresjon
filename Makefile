include /usr/lib/icu/Makefile.inc
#include /usr/lib/x86_64-linux-gnu/icu/Makefile.inc

CFLAGS = -Os -g -march=native
CC = clang
CXX = clang++

PROG = icu-pakk
SRC_DIR = src
OBJ_DIR = obj
STAT_DIR = stats

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

.PHONY: clean

all: $(PROG)

$(PROG): $(OBJ) | $(STAT_DIR)
	$(CXX) $^  -o $@ $(ICULIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

$(STAT_DIR):
	mkdir -p $@

clean:
	@$(RM) -rv $(OBJ_DIR) $(PROG) stats

# Old makefile

#$(ODIR)/%.o:	%.c
#	$(CC) -c -o $@ $< $(CFLAGS)

#simple c programs,no deps or objs:
#%: %.c
#	$(CC) $(CFLAGS) -o $@ $<
	
#all: $(PROGS)

#.PHONY: clean icu-clean

#clean:
#	rm -f obj/* $(PROGS)

#icu-clean:
#	rm -f obj/* icu-pakk
							
#aritkod: aritkod.c
#	$(CC) $(CFLAGS) -o $@ aritkod.c

#zap: zap.cpp zap.hpp
#	$(CXX) $(CFLAGS)  -lm -march=native -o $@ zap.cpp

#unzap: zap
#	rm -f unzap ; ln zap unzap
	
#case: case.c
#	$(CC) $(CFLAGS) -o $@ $<

#obj/aritkode.o: aritkode.cpp aritkode.hpp
#	$(CXX) $(CFLAGS) -c -o $@ $<

#obj/icu-pakk.o: icu-pakk.cpp aritkode.hpp tekstkompressor.hpp
#	$(CXX) -c $(CFLAGS) -o $@ $<

#obj/tekstkompressor.o: tekstkompressor.cpp tekstkompressor.hpp
#	$(CXX) -c $(CFLAGS) -o $@ $<

#icu-pakk: obj/icu-pakk.o obj/aritkode.o obj/tekstkompressor.o $(ODIR)
#	$(CXX) -o $@ $< obj/aritkode.o obj/tekstkompressor.o $(ICULIBS)

#pakkut: tekstkompressor.hpp	
