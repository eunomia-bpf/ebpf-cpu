CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -pthread
TARGETS = thermal_cap_control thermal_cap_benchmark

all: $(TARGETS)

thermal_cap_control: src/thermal_cap_control.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

thermal_cap_benchmark: src/thermal_cap_benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

.PHONY: all clean