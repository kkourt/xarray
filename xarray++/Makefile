.PHONY: all clean tests

CXX             = clang++
#CXX             = g++
INCLUDES        = -I$(shell fullpath ../include) -I$(shell fullpath .)
WARNINGS        = -Wall -Wshadow -Werror
OPTFLAGS        = -O2
DEFINES         = -D_GNU_SOURCE -D__STDC_FORMAT_MACROS
CXXFLAGS        = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -std=c++11  $(DEFINES)
CXXFLAGS       += -ggdb3

all: tests

## export all variables for recursive calls of make
export

tests:
	$(MAKE) -C tests/

clean:
	rm -f *.o
	$(MAKE) -C tests/ clean
