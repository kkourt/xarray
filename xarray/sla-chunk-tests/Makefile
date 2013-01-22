.PHONY: all
.PHONY: clean

SLA_DIR      = ..
TOPDIR       = ../..
CC           = gcc
CFLAGS       = -Wall -O2  -I$(SLA_DIR) -I$(TOPDIR)/include -I$(TOPDIR)/rle -std=c99 -D_GNU_SOURCE
CFLAGS      += -DNO_RLE_STATS

#CFLAGS      += -Wall -Wno-format-y2k -W -Wstrict-prototypes \
#               -Wmissing-prototypes -Wpointer-arith -Wreturn-type \
#               -Wcast-qual -Wwrite-strings -Wswitch -Wshadow \
#               -Wcast-align -Wuninitialized -ansi -pedantic \
#               -Wbad-function-cast -Wchar-subscripts -Winline \
#               -Wnested-externs -Wredundant-decls

SLA_OBJ  = $(SLA_DIR)/sla-chunk.o
.PHONY:  $(SLA_OBJ)
progs   := $(patsubst %.c,%,$(wildcard *.c))

all: $(progs) 

$(SLA_OBJ):
	make -C $(TOPDIR)

%.s: %.c
	$(CC) $(CFLAGS) -fverbose-asm -S $<

%.i: %.c
	$(CC) $(CFLAGS) -E $< > $@

%:: %.c $(SLA_OBJ)
	$(CC) $(CFLAGS) -lpthread $^ -o $@

clean:
	rm -rf $(progs)  *.s *.o
