.PHONY: all
.PHONY: clean

CC=gcc
CFLAGS= -Wall -O2 -g

all: prle_rec rle_rec rle_rec-SKEL

rle_rec: rle_rec.cilk
	$(CC) -xc -DNO_CILK $(CFLAGS) $< -o $@

rle_rec-SKEL: rle_rec-SKEL.cilk
	cilkc -cilk-critical-path $(CFLAGS) $< -o $@

prle_rec: rle_rec.cilk
	cilkc -cilk-critical-path $(CFLAGS) $< -o $@

clean:
	rm -f rle_rec prle_rec
