.PHONY: all clean

CILKDIR            = /usr/src/other/cilkplus.install

CC                 = $(CILKDIR)/bin/gcc
#CC                 = gcc
CFLAGS             = -Wall -O2 -Iinclude -I./rle -I./xarray -std=c99 -ggdb3 -D_GNU_SOURCE
CFLAGS            += -DNDEBUG
LDFLAGS            =

## tcmalloc
GPERFDIR           = /home/netos/tools/akourtis/gperftools/install
CFLAGS            += -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
CFLAGS            += -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free
LDFLAGS           += -ltcmalloc -L$(GPERFDIR)/lib -Xlinker -rpath=$(GPERFDIR)/lib

CILKCC             = $(CILKDIR)/bin/gcc
CILKCCFLAGS        = -fcilkplus $(CFLAGS)
                      # we don't need no stinkin LD_LIBRARY_PATH
CILKLDFLAGS        = -lcilkrts -Xlinker -rpath=$(CILKDIR)/lib  $(LDFLAGS)

hdrs  =$(wildcard rle/*.h)
hdrs +=$(wildcard xarray/*.h)
hdrs +=$(wildcard include/*.h)

all: rle/rle_rec rle/prle_rec                       \
     rle/rle_rec_mpools rle/prle_rec_mpools         \
     rle/prle_rec_xarray_da rle/rle_rec_xarray_da   \
     rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla \
     floorplan/floorplan

## xarray

xarray/dynarray.o: xarray/dynarray.c xarray/dynarray.h
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/sla-chunk.o: xarray/sla-chunk.c xarray/sla-chunk.h
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/xarray_dynarray.o: xarray/xarray_dynarray.c $(hdrs)
	$(CC) $(CFLAGS) $< -o $@ -c

## RLE

rle/prle_rec_xarray_sla: rle/prle_rec_xarray_sla.o xarray/sla-chunk.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

rle/rle_rec_xarray_sla: rle/rle_rec_xarray_sla.o xarray/sla-chunk.o
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@

rle/prle_rec_xarray_da: xarray/xarray_dynarray.o rle/prle_rec_xarray_da.o xarray/dynarray.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

rle/rle_rec_xarray_da: xarray/xarray_dynarray.o rle/rle_rec_xarray_da.o xarray/dynarray.o
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@

rle/prle_rec_xarray_da.o: rle/rle_rec_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DXARRAY_DA__ $< -o $@ -c

rle/prle_rec_xarray_sla.o: rle/rle_rec_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DXARRAY_SLA__ $< -o $@ -c

rle/rle_rec_xarray_da.o: rle/rle_rec_xarray.c $(hdrs)
	$(CC) $(CFLAGS) -DNO_CILK -DXARRAY_DA__ $< -o $@ -c

rle/rle_rec_xarray_sla.o: rle/rle_rec_xarray.c $(hdrs)
	$(CC) $(CFLAGS) -DNO_CILK -DXARRAY_SLA__ $< -o $@ -c

rle/prle_rec_mpools: rle/rle_rec_mpools.c
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $< -o $@

rle/rle_rec_mpools: rle/rle_rec_mpools.c
	$(CC) -DNO_CILK $(LDFLAGS) $(CFLAGS) $< -o $@

rle/prle_rec: rle/rle_rec.c
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $< -o $@

rle/rle_rec: rle/rle_rec.c
	$(CC) -DNO_CILK $(LDFLAGS) $(CFLAGS) $< -o $@

## floorplan

floorplan/floorplan.o: floorplan/floorplan.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DXARRAY_DA__ $< -o $@ -c

floorplan/floorplan: floorplan/floorplan.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

clean:
	rm -f rle/*.o xarray/*.o
	rm -f rle/rle_rec prle_rec    rle/rle_rec_mpools rle/prle_rec_mpools
	rm -f rle/prle_rec_xarray_da  rle/rle_rec_xarray_da
	rm -f rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla


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
