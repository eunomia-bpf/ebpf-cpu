# Master Makefile for all power management tools

SUBDIRS = cpu-freq cpu-cstate thermal-cap gpu-devfreq

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Run all benchmarks
benchmark: all
	@echo "Running CPU Frequency Benchmark..."
	cd cpu-freq && sudo ./cpu_freq_benchmark
	@echo "\nRunning CPU C-State Benchmark..."
	cd cpu-cstate && sudo ./cpu_cstate_benchmark
	@echo "\nRunning Thermal Cap Benchmark..."
	cd thermal-cap && sudo ./thermal_cap_benchmark
	@echo "\nRunning GPU DevFreq Benchmark..."
	cd gpu-devfreq && sudo ./gpu_devfreq_benchmark

# Quick test of all tools
test: all
	@echo "Testing CPU Frequency Control..."
	cd cpu-freq && sudo ./cpu_freq_control list-freq
	@echo "\nTesting CPU C-State Control..."
	cd cpu-cstate && sudo ./cpu_cstate_control list
	@echo "\nTesting Thermal Cap Control..."
	cd thermal-cap && sudo ./thermal_cap_control list
	@echo "\nTesting GPU DevFreq Control..."
	cd gpu-devfreq && sudo ./gpu_devfreq_control list