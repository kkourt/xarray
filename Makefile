.PHONY: all
.PHONY: clean

CC=gcc
CFLAGS= -Wall -O2 -I./xarray -std=c99 -ggdb3 #-DNDEBUG
CILKCC=/usr/src/other/cilkplus.install/bin/gcc
CILKCCFLAGS=-fcilkplus $(CFLAGS)
# we don't need no stinkin LD_LIBRARY_PATH
CILKLDFLAGS=-lcilkrts -Xlinker -rpath=/usr/src/other/cilkplus.install/lib

hdrs  =$(wildcard *.h)
hdrs +=$(wildcard xarray/*.h)

all: rle_rec prle_rec                     \
     rle_rec_mpools prle_rec_mpools       \
     prle_rec_xarray_da rle_rec_xarray_da \
     prle_rec_xarray_sla rle_rec_xarray_sla

prle_rec_xarray_sla: prle_rec_xarray_sla.o xarray/sla-chunk.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

rle_rec_xarray_sla: rle_rec_xarray_sla.o xarray/sla-chunk.o
	$(CC) $(CFLAGS) $^ -o $@

prle_rec_xarray_da: xarray/xarray_dynarray.o prle_rec_xarray_da.o xarray/dynarray.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

rle_rec_xarray_da: xarray/xarray_dynarray.o rle_rec_xarray_da.o xarray/dynarray.o
	$(CC) $(CFLAGS) $^ -o $@

xarray/dynarray.o: xarray/dynarray.c xarray/dynarray.h
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/sla-chunk.o: xarray/sla-chunk.c xarray/sla-chunk.h
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

prle_rec_xarray_da.o: rle_rec_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DXARRAY_DA__ $< -o $@ -c

prle_rec_xarray_sla.o: rle_rec_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DXARRAY_SLA__ $< -o $@ -c

rle_rec_xarray_da.o: rle_rec_xarray.c $(hdrs)
	$(CC) $(CFLAGS) -DNO_CILK -DXARRAY_DA__ $< -o $@ -c

rle_rec_xarray_sla.o: rle_rec_xarray.c $(hdrs)
	$(CC) $(CFLAGS) -DNO_CILK -DXARRAY_SLA__ $< -o $@ -c

xarray/xarray_dynarray.o: xarray/xarray_dynarray.c $(hdrs)
	$(CC) $(CFLAGS) $< -o $@ -c

prle_rec_mpools: rle_rec_mpools.c
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $< -o $@

rle_rec_mpools: rle_rec_mpools.c
	$(CC) -DNO_CILK $(CFLAGS) $< -o $@

prle_rec: rle_rec.c
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $< -o $@

rle_rec: rle_rec.c
	$(CC) -DNO_CILK $(CFLAGS) $< -o $@

clean:
	rm -f *.o xarray/*.o rle_rec prle_rec rle_rec_mpools prle_rec_mpools
	rm -f prle_rec_xarray_da rle_rec_xarray_da
	rm -f prle_rec_xarray_sla rle_rec_xarray_sla


## Old versions based on cilk, compiled as C

#rle_rec: rle_rec.cilk
#	$(CC) -xc -DNO_CILK $(CFLAGS) $< -o $@
#
#rle_rec-mempools: rle_rec-mempools.cilk
#	$(CC) -xc -DNO_CILK $(CFLAGS) $< -o $@


## Old versions based on cilk, which is now borken

#rle_rec-SKEL: rle_rec-SKEL.cilk
#	cilkc -cilk-critical-path $(CFLAGS) $< -o $@
#
#prle_rec: rle_rec.cilk
#	cilkc -cilk-critical-path $(CFLAGS) $< -o $@
#
#prle_rec-mempools: rle_rec-mempools.cilk
#	cilkc -cilk-critical-path $(CFLAGS) $< -o $@
#clean:
	#rm -f rle_rec prle_rec prle_rec-mempools rle_rec-mempools prle_rec_mpools *.o
