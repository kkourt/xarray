.PHONY: all clean

progs   := $(patsubst %.cc,%,$(wildcard *.cc))

all: $(progs) 

#%.s: %.c
#	$(CC) $(CFLAGS) -fverbose-asm -S $<
#
#%.i: %.c
#	$(CC) $(CFLAGS) -E $< > $@
#
#%: %.c
#	$(CC) $(CFLAGS) -lpthread $^ -o $@
#
#%: %.cc
#	$(CXX) $(CXXFLAGS) $^ -o $@
#
#%.o: %.cc
#	$(CXX) $(CXXFLAGS) $< -c -o $@

clean:
	rm -rf $(progs)  *.s *.o
