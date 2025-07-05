CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -pthread
TARGETS = cpu_freq_control cpu_freq_benchmark

all: $(TARGETS)

cpu_freq_control: src/cpu_freq_control.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

cpu_freq_benchmark: src/cpu_freq_benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean