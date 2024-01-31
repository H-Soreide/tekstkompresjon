include /usr/lib/icu/Makefile.inc

CFLAGS=-Os -g -march=native
CC=clang
CXX=clang++
PROGS=aritkod zap unzap icu-pakk case bw mtf rl
#ODIR=obj

#$(ODIR)/%.o:	%.c
#	$(CC) -c -o $@ $< $(CFLAGS)

#simple c programs,no deps or objs:
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
	
all: $(PROGS)

.PHONY: clean icu-clean

clean:
	rm -f obj/* $(PROGS)

icu-clean:
	rm -f obj/* icu-pakk
							
aritkod: aritkod.c
	$(CC) $(CFLAGS) -o $@ aritkod.c

zap: zap.cpp zap.hpp
	$(CXX) $(CFLAGS)  -lm -march=native -o $@ zap.cpp

unzap: zap
	rm -f unzap ; ln zap unzap
	
case: case.c
	$(CC) $(CFLAGS) -o $@ $<

obj/aritkode.o: aritkode.cpp aritkode.hpp
	$(CXX) $(CFLAGS) -c -o $@ $<

obj/icu-pakk.o: icu-pakk.cpp aritkode.hpp tekstkompressor.hpp
	$(CXX) -c $(CFLAGS) -o $@ $<

obj/tekstkompressor.o: tekstkompressor.cpp tekstkompressor.hpp
	$(CXX) -c $(CFLAGS) -o $@ $<

icu-pakk:	obj/icu-pakk.o obj/aritkode.o obj/tekstkompressor.o
	$(CXX) -o $@ $< obj/aritkode.o obj/tekstkompressor.o $(ICULIBS)

#pakkut: tekstkompressor.hpp	
