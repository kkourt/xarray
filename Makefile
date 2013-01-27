.PHONY: all clean

USE_TCMALLOC   ?= 1
DEBUG_BUILD    ?= 0
NOSTATS_BUILD  ?= 0


CILKDIR            = /usr/src/other/cilkplus.install
#CILKDIR            = /usr/src/other/gcc-cilk.git/install

CC                 = $(CILKDIR)/bin/gcc
#CC                 = gcc
INCLUDES            = -I./verp -I./include -I./rle -I./xarray -I./floorplan
WARNINGS            =  -Wall -Wshadow
OPTFLAGS            = -O2
CFLAGS              = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -std=c99 -ggdb3 -D_GNU_SOURCE
CXXFLAGS            = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -ggdb3 -D_GNU_SOURCE -D__STDC_FORMAT_MACROS # C++ (PRIu64)
ifeq (0, $(DEBUG_BUILD))
CFLAGS            += -DNDEBUG
endif
ifeq (1, $(NOSTATS_BUILD))
CFLAGS            += -DNO_RLE_STATS
endif
LDFLAGS            =

sla_objs           = xarray/sla-chunk.o
verp_objs          = verp/verp.o verp/ver.o
xvarray_objs_sla   = $(verp_objs) $(sla_objs)

CILKCC             = $(CILKDIR)/bin/gcc
CILKCPP            = $(CILKDIR)/bin/g++
CILKCCFLAGS        = -fcilkplus $(CFLAGS)
                      # we don't need no stinkin LD_LIBRARY_PATH
CILKLDFLAGS        = -lcilkrts -Xlinker -rpath=$(CILKDIR)/lib  $(LDFLAGS)

ifeq (1,$(USE_TCMALLOC))
	CFLAGS    += -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -ltcmalloc
	# check for custom installation of gperftools
	GPERFDIR   = /home/netos/tools/akourtis/gperftools/install
	LDFLAGS    = -ltcmalloc
	ifeq ($(strip $(wildcard $(GPERFDIR))),$(GPERFDIR))
		LDFLAGS += -L$(GPERFDIR)/lib -Xlinker -rpath=$(GPERFDIR)/lib
	endif
endif

## create dependencies for all headers
## TODO: http://stackoverflow.com/questions/2394609/makefile-header-dependencies
hdrs  =$(wildcard rle/*.h)
hdrs +=$(wildcard xarray/*.h)
hdrs +=$(wildcard include/*.h)
hdrs +=$(wildcard verp/*.h)
hdrs +=$(wildcard floorplan/*.h)

all: rle/rle_rec rle/prle_rec                       \
     rle/rle_rec_mpools rle/prle_rec_mpools         \
     rle/prle_rec_xarray_da rle/rle_rec_xarray_da   \
     rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla \
     floorplan/floorplan-serial floorplan/floorplan \
     floorplan/floorplan_sla

     #xarray/xvarray-tests/branch_sla


## xarray

xarray/dynarray.o: xarray/dynarray.c xarray/dynarray.h $(hdrs)
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/sla-chunk.o: xarray/sla-chunk.c xarray/sla-chunk.h $(hdrs)
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
	$(CILKCC) $(CILKCCFLAGS) $< -o $@ -c

floorplan/floorplan: floorplan/floorplan.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

floorplan/reducer_test.o: floorplan/reducer_test.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) $< -o $@ -c

floorplan/reducer_test: floorplan/reducer_test.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

floorplan/reducer_test_cpp.o: floorplan/reducer_test_cpp.cpp $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) $< -o $@ -c

floorplan/reducer_test_cpp: floorplan/reducer_test_cpp.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

floorplan/floorplan-serial: floorplan/floorplan-serial.c
	$(CC) $(LDFLAGS) $(CFLAGS) $< -o $@

floorplan/floorplan_sla.o: floorplan/floorplan.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DFLOORPLAN_XVARRAY -DXARRAY_SLA__  $< -o $@ -c

floorplan/floorplan_sla: floorplan/floorplan_sla.o $(sla_objs) $(verp_objs)
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

## verp

verp/verp.o verp/ver.o: %.o: %.c $(hdrs)
	$(CC) $(LDFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f xarray/*.o
	#
	rm -f rle/*.o
	rm -f rle/rle_rec prle_rec    rle/rle_rec_mpools rle/prle_rec_mpools
	rm -f rle/prle_rec_xarray_da  rle/rle_rec_xarray_da
	rm -f rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla
	#
	rm -f floorplan/*.o
	rm -f floorplan/floorplan floorplan/floorplan-serial xarray/*.o
	#
	rm -f verp/*.o


## xvarray tests

xarray/xvarray-tests/branch_sla.o: xarray/xvarray-tests/branch.c $(hdrs)
	$(CC) $(CFLAGS) -DXARRAY_SLA__ $< -c -o $@

xarray/xvarray-tests/branch_sla: xarray/xvarray-tests/branch_sla.o $(xvarray_objs_sla)
	$(CC) $(LDFLAGS) $^ -o $@



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
