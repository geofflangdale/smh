
.SUFFIXES:
#
.SUFFIXES: .cpp .o .c .h


.PHONY: clean cleandist

CXXFLAGS =  -std=c++17 -O3 -march=native -Wall -Wextra -Wshadow 

EXECUTABLES=smh



all: $(EXECUTABLES)

smh: main.cpp smh.h common_defs.h
	$(CXX) $(CXXFLAGS) -o smh main.cpp


clean:
	rm -f $(EXECUTABLES)

cleandist:
	rm -f $(EXECUTABLES)
