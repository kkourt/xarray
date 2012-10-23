.PHONY: all
.PHONY: clean

CC=gcc
CFLAGS= -Wall -O2 -DNDEBUG
CILKCC=/usr/src/other/cilkplus.install/bin/gcc
CILKCCFLAGS=-fcilkplus $(CFLAGS)
# we don't need no stinkin LD_LIBRARY_PATH
CILKLDFLAGS=-lcilkrts -Xlinker -rpath=/usr/src/other/cilkplus.install/lib

all: rle_rec_mpools prle_rec_mpools

prle_rec_mpools: rle_rec_mpools.c
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $< -o $@

rle_rec_mpools: rle_rec_mpools.c
	$(CC) -DNO_CILK $(CFLAGS) $< -o $@

## Old versions based on cilk, compiled as C

rle_rec: rle_rec.cilk
	$(CC) -xc -DNO_CILK $(CFLAGS) $< -o $@

rle_rec-mempools: rle_rec-mempools.cilk
	$(CC) -xc -DNO_CILK $(CFLAGS) $< -o $@


## Old versions based on cilk, which is now borken

rle_rec-SKEL: rle_rec-SKEL.cilk
	cilkc -cilk-critical-path $(CFLAGS) $< -o $@

prle_rec: rle_rec.cilk
	cilkc -cilk-critical-path $(CFLAGS) $< -o $@

prle_rec-mempools: rle_rec-mempools.cilk
	cilkc -cilk-critical-path $(CFLAGS) $< -o $@

clean:
	rm -f *.o rle_rec_mpools prle_rec_mpools
	#rm -f rle_rec prle_rec prle_rec-mempools rle_rec-mempools prle_rec_mpools *.o
