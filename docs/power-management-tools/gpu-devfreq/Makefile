CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -pthread
LDFLAGS = -lGL -lX11
TARGETS = gpu_devfreq_control gpu_devfreq_benchmark

all: $(TARGETS)

gpu_devfreq_control: src/gpu_devfreq_control.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

gpu_devfreq_benchmark: src/gpu_devfreq_benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean