.PHONY: all
.PHONY: clean

SLA_DIR      = ..
SLA_MAKE_DIR = ../..
CC           = gcc
CFLAGS       = -Wall -O2  -I$(SLA_DIR) -std=c99

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
	make -C $(SLA_MAKE_DIR)

%.s: %.c
	$(CC) $(CFLAGS) -fverbose-asm -S $<

%.i: %.c
	$(CC) $(CFLAGS) -E $< > $@

%:: %.c $(SLA_OBJ)
	$(CC) $(CFLAGS) -lpthread $^ -o $@

clean:
	rm -rf $(progs)  *.s *.o
