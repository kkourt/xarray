.PHONY: all clean

USE_TCMALLOC   ?= 1
DEBUG_BUILD    ?= 0
NOSTATS_BUILD  ?= 0
SLA_MAX_LEVEL  ?= 5

CILKDIR            = /usr/src/other/cilkplus.install
#CILKDIR            = /usr/src/other/gcc-cilk.git/install

CC                  = $(CILKDIR)/bin/gcc
CPP                 = $(CILKDIR)/bin/g++
CILKCC              = $(CILKDIR)/bin/gcc
CILKCPP             = $(CILKDIR)/bin/g++
LD                  = ld
#CC                 = gcc
INCLUDES            = -I./verp -I./include -I./rle -I./xarray -I./floorplan
WARNINGS            =  -Wall -Wshadow -Werror
OPTFLAGS            = -O2
CFLAGS              = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -std=c99 -ggdb3 -D_GNU_SOURCE -DSLA_MAX_LEVEL=$(SLA_MAX_LEVEL)
CXXFLAGS            = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -ggdb3 -D_GNU_SOURCE -D__STDC_FORMAT_MACROS # C++ (PRIu64)
ifeq (0, $(DEBUG_BUILD))
CFLAGS            += -DNDEBUG
endif
ifeq (1, $(NOSTATS_BUILD))
CFLAGS            += -DNO_RLE_STATS -DNO_SUM_STATS
endif
LDFLAGS            =

sla_objs           = xarray/sla-chunk.o
verp_objs          = verp/verp.o verp/ver.o
xvarray_objs_sla   = $(verp_objs) $(sla_objs)

CILKCCFLAGS        = -fcilkplus $(CFLAGS)
                      # we don't need no stinkin LD_LIBRARY_PATH
CILKLDFLAGS        = -lcilkrts -Xlinker
CILKLDFLAGS       += -rpath=$(CILKDIR)/lib
CILKLDFLAGS       += $(LDFLAGS)

#CFLAGS            += -fprofile-arcs -ftest-coverage
#LDFLAGS           += -lgov

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
     rle/prle_rec_xarray_da rle/rle_rec_xarray_da  \
     rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla \
     rle/prle_rec_xarray_rpa rle/rle_rec_xarray_rpa \
     sum/sum_omp \
     sum/psum_xarray_da sum/psum_xarray_sla sum/psum_xarray_rpa \
     sum/sum_xarray_da sum/sum_xarray_sla sum/sum_xarray_rpa \
     xarray/tests/slice_rpa xarray/tests/slice_sla xarray/tests/slice_da

     #xarray/xvarray-tests/branch_sla
     #floorplan/floorplan-serial floorplan/floorplan
     #floorplan/floorplan_sla

# sla -> -DXARRAY_SLA__
# da  -> -DXARRAY_DA__
# ... -> ...
define XARR_CFLAGS
-DXARRAY_$(shell echo $1 | tr a-z A-Z)__
endef

## xarray
# -----------------------------------------------------------------------------

# da
xarray/dynarray.o: xarray/dynarray.c xarray/dynarray.h $(hdrs)
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/xarray_dynarray.o: xarray/xarray_dynarray.c $(hdrs)
	$(CC) $(CFLAGS) $< -o $@ -c

xarray/xarray_da.o: xarray/xarray_dynarray.o xarray/dynarray.o
	$(LD) -i $^ -o $@

# sla
xarray/sla-chunk.o: xarray/sla-chunk.c xarray/sla-chunk.h $(hdrs)
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/xarray_sla.o: xarray/sla-chunk.o
	$(LD) -i $^ -o $@

# rpa
xarray/rope_array.o: xarray/rope_array.c xarray/rope_array.h
	$(CC) $(CFLAGS) -std=gnu99 $< -o $@ -c

xarray/xarray_rpa.o: xarray/rope_array.o
	$(LD) -i $^ -o $@

## Tests
# -----------------------------------------------------------------------------

xarray/tests/slice_%: xarray/xarray_%.o xarray/tests/slice.c $(hdrs)
	$(CC) $(CFLAGS) $(call XARR_CFLAGS,$*) $(LDFLAGS) $< xarray/tests/slice.c -o $@

## SUM
# -----------------------------------------------------------------------------

# openMP version
sum/sum_omp: sum/sum_openmp.c $(hdrs)
	$(CC) $(CFLAGS) $(LDFLAGS) -fopenmp $< -o $@

# parallel version
sum/psum_xarray_%.o: sum/sum_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) $(call XARR_CFLAGS,$*)  $< -o $@ -c

sum/psum_xarray_%: sum/psum_xarray_%.o xarray/xarray_%.o
	$(CILKCC) $(CILKLDFLAGS) $^ -o $@

# serial version
sum/sum_xarray_%.o: sum/sum_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) -DNO_CILK $(call XARR_CFLAGS,$*) $< -o $@ -c

sum/sum_xarray_%: sum/sum_xarray_%.o xarray/xarray_%.o
	$(CILKCC) $(CILKLDFLAGS) $^ -o $@

## RLE
# -----------------------------------------------------------------------------

rle/rle_rec_xarray_%.o: rle/rle_rec_xarray.c $(hdrs)
	$(CC) $(CFLAGS) -DNO_CILK $(call XARR_CFLAGS,$*) $< -o $@ -c

rle/rle_rec_xarray_%: rle/rle_rec_xarray_%.o xarray/xarray_%.o
	$(CC) $(LDFLAGS) $(CFLAGS) $^ -o $@

rle/prle_rec_xarray_%.o: rle/rle_rec_xarray.c $(hdrs)
	$(CILKCC) $(CILKCCFLAGS) $(call XARR_CFLAGS,$*) $< -o $@ -c

rle/prle_rec_xarray_%: rle/prle_rec_xarray_%.o xarray/xarray_%.o
	$(CILKCC) $(CILKCCFLAGS) $(CILKLDFLAGS) $^ -o $@

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
	rm -f rle/rle_rec rle/prle_rec  rle/rle_rec_mpools rle/prle_rec_mpools
	rm -f rle/prle_rec_xarray_da  rle/rle_rec_xarray_da
	rm -f rle/prle_rec_xarray_sla rle/rle_rec_xarray_sla
	#
	rm -f floorplan/*.o
	rm -f floorplan/floorplan floorplan/floorplan-serial xarray/*.o
	#
	rm -f verp/*.o
	#
	rm -f sum/*.o
	rm -rf sum/psum_xarray_da sum/psum_xarray_sla sum/sum_omp
	rm -rf sum/sum_xarray_da sum/sum_xarray_sla


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
