.PHONY: all clean

CXX             = clang++
#CXX             = g++
INCLUDES        = -I ../include
WARNINGS        = -Wall -Wshadow -Werror
#OPTFLAGS        = -O2
CXXFLAGS        = $(INCLUDES) $(WARNINGS) $(OPTFLAGS) -std=c++11 -ggdb3 -D_GNU_SOURCE -D__STDC_FORMAT_MACROS # C++ (PRIu64)

all: sl_range

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -c -o $@

sl_range: sl_range.o
	$(CXX) $< -o $@

clean:
	rm -f *.o sl_range
