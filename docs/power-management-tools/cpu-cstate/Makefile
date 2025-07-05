CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -pthread
TARGETS = cpu_cstate_control cpu_cstate_benchmark

all: $(TARGETS)

cpu_cstate_control: src/cpu_cstate_control.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

cpu_cstate_benchmark: src/cpu_cstate_benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean