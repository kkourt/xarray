.PHONY: all
.PHONY: clean

RPA_DIR      = ..
TOPDIR       = ../..
CC           = gcc
CFLAGS       = -Wall -O0  -ggdb -I$(RPA_DIR) -I$(TOPDIR)/include -I$(TOPDIR)/rle -std=c99 -D_GNU_SOURCE
CFLAGS      += -DNO_RLE_STATS

RPA_OBJ  =  $(RPA_DIR)/rope_array.o
progs   := $(patsubst %.c,%,$(wildcard *.c))

all: $(progs) 

.PHONY:  $(RPA_OBJ)
$(RPA_OBJ):
	make -C $(TOPDIR) xarray/rope_array.o

%.s: %.c
	$(CC) $(CFLAGS) -fverbose-asm -S $<

%.i: %.c
	$(CC) $(CFLAGS) -E $< > $@

%:: %.c $(RPA_OBJ) ../rope_array.h $(RPA_OBJ)
	$(CC) $(CFLAGS) -lpthread $< $(RPA_OBJ) -o $@

clean:
	rm -rf $(progs)  *.s *.o
