# Hardware Knobs Test Suite Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -O2 -g -D_GNU_SOURCE
LDFLAGS = -pthread

# Directories
COMMON_DIR = common
RDT_DIR = rdt
PREFETCH_DIR = prefetch
SMT_DIR = smt
UNCORE_DIR = uncore
RAPL_DIR = rapl
CXL_DIR = cxl

# Output directory
BUILD_DIR = build

# Common source files
COMMON_SRCS = $(COMMON_DIR)/common.c $(COMMON_DIR)/msr_utils.c
COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))

# Prefetch common files
PREFETCH_COMMON_SRCS = $(PREFETCH_DIR)/prefetch_common.c
PREFETCH_COMMON_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(PREFETCH_COMMON_SRCS))

# Test programs
RDT_PROGS = $(BUILD_DIR)/rdt_test $(BUILD_DIR)/rdt_monitor
PREFETCH_PROGS = $(BUILD_DIR)/prefetch_test $(BUILD_DIR)/prefetch_bench
SMT_PROGS = $(BUILD_DIR)/smt_test $(BUILD_DIR)/smt_bench
UNCORE_PROGS = $(BUILD_DIR)/uncore_test
RAPL_PROGS = $(BUILD_DIR)/rapl_test
CXL_PROGS = $(BUILD_DIR)/cxl_test

ALL_PROGS = $(RDT_PROGS) $(PREFETCH_PROGS) $(SMT_PROGS) $(UNCORE_PROGS) $(RAPL_PROGS) $(CXL_PROGS)

# Default target
.PHONY: all
all: $(BUILD_DIR) $(ALL_PROGS)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/$(COMMON_DIR)
	mkdir -p $(BUILD_DIR)/$(RDT_DIR)
	mkdir -p $(BUILD_DIR)/$(PREFETCH_DIR)
	mkdir -p $(BUILD_DIR)/$(SMT_DIR)
	mkdir -p $(BUILD_DIR)/$(UNCORE_DIR)
	mkdir -p $(BUILD_DIR)/$(RAPL_DIR)
	mkdir -p $(BUILD_DIR)/$(CXL_DIR)

# Common object files
$(BUILD_DIR)/$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Prefetch common object files
$(BUILD_DIR)/$(PREFETCH_DIR)/%.o: $(PREFETCH_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# RDT programs
$(BUILD_DIR)/rdt_test: $(RDT_DIR)/rdt_test.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/rdt_monitor: $(RDT_DIR)/rdt_monitor.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

# Prefetch programs
$(BUILD_DIR)/prefetch_test: $(PREFETCH_DIR)/prefetch_test.c $(COMMON_OBJS) $(PREFETCH_COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(PREFETCH_COMMON_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/prefetch_bench: $(PREFETCH_DIR)/prefetch_bench.c $(COMMON_OBJS) $(PREFETCH_COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(PREFETCH_COMMON_OBJS) -o $@ $(LDFLAGS)

# SMT programs
$(BUILD_DIR)/smt_test: $(SMT_DIR)/smt_test.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/smt_bench: $(SMT_DIR)/smt_bench.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

# Uncore programs
$(BUILD_DIR)/uncore_test: $(UNCORE_DIR)/uncore_test.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

# RAPL programs
$(BUILD_DIR)/rapl_test: $(RAPL_DIR)/rapl_test.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

# CXL programs
$(BUILD_DIR)/cxl_test: $(CXL_DIR)/cxl_test.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) -o $@ $(LDFLAGS)

# Individual component targets
.PHONY: rdt prefetch smt uncore rapl cxl
rdt: $(BUILD_DIR) $(RDT_PROGS)
prefetch: $(BUILD_DIR) $(PREFETCH_PROGS)
smt: $(BUILD_DIR) $(SMT_PROGS)
uncore: $(BUILD_DIR) $(UNCORE_PROGS)
rapl: $(BUILD_DIR) $(RAPL_PROGS)
cxl: $(BUILD_DIR) $(CXL_PROGS)

# Test targets
.PHONY: test test-rdt test-prefetch test-smt test-uncore test-rapl test-cxl
test: test-rdt test-prefetch test-smt test-uncore test-rapl test-cxl

test-rdt: $(RDT_PROGS)
	@echo "=== Running RDT Tests ==="
	@echo "Checking for root privileges and MSR support..."
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for MSR access"; \
		echo "Please run: sudo make test-rdt"; \
		exit 1; \
	fi
	@if [ ! -c /dev/cpu/0/msr ]; then \
		echo "Loading MSR module..."; \
		modprobe msr || (echo "ERROR: Failed to load MSR module" && exit 1); \
	fi
	$(BUILD_DIR)/rdt_test
	@echo "Starting RDT monitor for 5 seconds..."
	timeout 5 $(BUILD_DIR)/rdt_monitor 5 || true

test-prefetch: $(PREFETCH_PROGS)
	@echo "=== Running Prefetch Tests ==="
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for MSR access"; \
		echo "Please run: sudo make test-prefetch"; \
		exit 1; \
	fi
	@if [ ! -c /dev/cpu/0/msr ]; then \
		echo "Loading MSR module..."; \
		modprobe msr || (echo "ERROR: Failed to load MSR module" && exit 1); \
	fi
	$(BUILD_DIR)/prefetch_test
	@echo "Running prefetch benchmark..."
	$(BUILD_DIR)/prefetch_bench

test-smt: $(SMT_PROGS)
	@echo "=== Running SMT Tests ==="
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for SMT control"; \
		echo "Please run: sudo make test-smt"; \
		exit 1; \
	fi
	$(BUILD_DIR)/smt_test
	@echo "Running SMT benchmark..."
	$(BUILD_DIR)/smt_bench

test-uncore: $(UNCORE_PROGS)
	@echo "=== Running Uncore Tests ==="
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for uncore frequency control"; \
		echo "Please run: sudo make test-uncore"; \
		exit 1; \
	fi
	$(BUILD_DIR)/uncore_test

test-rapl: $(RAPL_PROGS)
	@echo "=== Running RAPL Tests ==="
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for RAPL access"; \
		echo "Please run: sudo make test-rapl"; \
		exit 1; \
	fi
	@if [ ! -c /dev/cpu/0/msr ]; then \
		echo "Loading MSR module..."; \
		modprobe msr || (echo "ERROR: Failed to load MSR module" && exit 1); \
	fi
	$(BUILD_DIR)/rapl_test

test-cxl: $(CXL_PROGS)
	@echo "=== Running CXL Tests ==="
	@if [ "$$(id -u)" -ne 0 ]; then \
		echo "ERROR: Root privileges required for CXL access"; \
		echo "Please run: sudo make test-cxl"; \
		exit 1; \
	fi
	$(BUILD_DIR)/cxl_test

# System information
.PHONY: sysinfo
sysinfo:
	@echo "=== System Information ==="
	@echo "CPU Information:"
	@cat /proc/cpuinfo | grep -E "model name|vendor_id|cpu family|flags" | head -4
	@echo ""
	@echo "Available CPU features relevant to hardware knobs:"
	@echo -n "  RDT: "; grep -q "rdt_a" /proc/cpuinfo && echo "Supported" || echo "Not supported"
	@echo -n "  SMT/HT: "; grep -q "ht" /proc/cpuinfo && echo "Supported" || echo "Not supported"
	@echo -n "  RAPL: "; [ -d "/sys/class/powercap/intel-rapl" ] && echo "Available" || echo "Not available"
	@echo -n "  Uncore Freq: "; [ -d "/sys/devices/system/cpu/intel_uncore_frequency" ] && echo "Available" || echo "Not available"
	@echo -n "  CXL: "; [ -d "/sys/bus/cxl" ] && echo "Available" || echo "Not available"
	@echo ""
	@echo "MSR module:"
	@lsmod | grep msr || echo "  MSR module not loaded"
	@echo ""
	@echo "CPU count: $$(nproc)"
	@echo "NUMA nodes: $$(ls /sys/devices/system/node/ | grep node | wc -l)"

# Installation target
.PHONY: install
install: all
	@echo "Installing hardware knobs test suite..."
	install -d /usr/local/bin/hardware-knobs
	install -m 755 $(ALL_PROGS) /usr/local/bin/hardware-knobs/
	@echo "Installed to /usr/local/bin/hardware-knobs/"
	@echo ""
	@echo "Usage examples:"
	@echo "  sudo /usr/local/bin/hardware-knobs/rdt_test"
	@echo "  sudo /usr/local/bin/hardware-knobs/prefetch_test"
	@echo "  sudo /usr/local/bin/hardware-knobs/smt_test"

# Uninstallation target
.PHONY: uninstall
uninstall:
	rm -rf /usr/local/bin/hardware-knobs/
	@echo "Uninstalled hardware knobs test suite"

# Clean targets
.PHONY: clean clean-all
clean:
	rm -rf $(BUILD_DIR)

clean-all: clean
	rm -f *.log *.tmp

# Help target
.PHONY: help
help:
	@echo "Hardware Knobs Test Suite Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all test programs"
	@echo "  rdt          - Build RDT test programs"
	@echo "  prefetch     - Build prefetch test programs"
	@echo "  smt          - Build SMT test programs"
	@echo "  uncore       - Build uncore test programs"
	@echo "  rapl         - Build RAPL test programs"
	@echo "  cxl          - Build CXL test programs"
	@echo ""
	@echo "  test         - Run all tests (requires root)"
	@echo "  test-rdt     - Run RDT tests only"
	@echo "  test-prefetch- Run prefetch tests only"
	@echo "  test-smt     - Run SMT tests only"
	@echo "  test-uncore  - Run uncore tests only"
	@echo "  test-rapl    - Run RAPL tests only"
	@echo "  test-cxl     - Run CXL tests only"
	@echo ""
	@echo "  sysinfo      - Show system hardware knob support"
	@echo "  install      - Install to /usr/local/bin/hardware-knobs/"
	@echo "  uninstall    - Remove installation"
	@echo "  clean        - Remove build files"
	@echo "  clean-all    - Remove all generated files"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  - Root privileges for most tests"
	@echo "  - MSR kernel module (modprobe msr)"
	@echo "  - Intel CPU for most features"
	@echo "  - Hardware support for specific features"
	@echo ""
	@echo "Examples:"
	@echo "  make all                    # Build everything"
	@echo "  make sysinfo               # Check hardware support"
	@echo "  sudo make test-rdt         # Test RDT functionality"
	@echo "  sudo make test             # Run full test suite"

# Debug target
.PHONY: debug
debug: CFLAGS += -DDEBUG -O0
debug: all

# Default help when no target is specified
.DEFAULT_GOAL := help