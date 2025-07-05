#!/bin/bash

# Hardware Knobs Test Suite Runner
# Comprehensive test runner for hardware knobs with CPU feature detection

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper functions
print_header() {
    echo -e "${BLUE}=================================================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=================================================================================${NC}"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
}

# Helper to check CPU flag support
check_cpu_flag() {
    local flag=$1
    local name=$2
    if grep -qw "$flag" /proc/cpuinfo; then
        print_success "$name: Supported"
    else
        print_warning "$name: Not supported"
    fi
}

# Check if running as root
check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root for hardware access"
        print_info "Usage: sudo $0 [OPTIONS]"
        exit 1
    fi
}

# Check if build directory exists
check_build() {
    if [[ ! -d "build" ]]; then
        print_error "Build directory not found. Please run 'make' first"
        exit 1
    fi
}

# Load required kernel modules
load_modules() {
    print_info "Loading required kernel modules..."
    
    # Load MSR module
    if ! lsmod | grep -q "^msr "; then
        print_info "Loading MSR module..."
        if modprobe msr 2>/dev/null; then
            print_success "MSR module loaded"
        else
            print_warning "Could not load MSR module"
        fi
    else
        print_success "MSR module already loaded"
    fi
    
    # Check MSR device access
    if [[ -c /dev/cpu/0/msr ]]; then
        print_success "MSR device access available"
    else
        print_warning "MSR device access not available"
    fi
}

# Detect CPU features
detect_cpu_features() {
    print_header "CPU Feature Detection"
    
    # Basic CPU info
    print_info "CPU Information:"
    local cpu_model=$(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    local cpu_vendor=$(grep "vendor_id" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    local cpu_family=$(grep "cpu family" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
    local cpu_count=$(nproc)
    
    echo "  Model: $cpu_model"
    echo "  Vendor: $cpu_vendor"
    echo "  Family: $cpu_family"
    echo "  CPU Count: $cpu_count"
    
    # NUMA topology
    local numa_nodes=$(ls /sys/devices/system/node/ 2>/dev/null | grep -c node)
    echo "  NUMA Nodes: $numa_nodes"
    
    echo
    print_info "Hardware Knob Support:"
    
    # Check RDT (Resource Director Technology)
    if grep -q "rdt_a" /proc/cpuinfo; then
        print_success "RDT (Resource Director Technology): Supported"
        RDT_SUPPORTED=1
    else
        print_warning "RDT (Resource Director Technology): Not supported"
        RDT_SUPPORTED=0
    fi
    
    # Check SMT/Hyper-Threading
    if grep -q "ht" /proc/cpuinfo; then
        print_success "SMT/Hyper-Threading: Supported"
        SMT_SUPPORTED=1
    else
        print_warning "SMT/Hyper-Threading: Not supported"
        SMT_SUPPORTED=0
    fi
    
    # Check for SMT control interface
    if [[ -f /sys/devices/system/cpu/smt/control ]]; then
        local smt_state=$(cat /sys/devices/system/cpu/smt/control 2>/dev/null || echo "unknown")
        echo "  SMT State: $smt_state"
    fi
    
    # Check RAPL (Running Average Power Limit)
    if [[ -d /sys/class/powercap/intel-rapl ]]; then
        print_success "RAPL (Power Management): Available"
        RAPL_SUPPORTED=1
    else
        print_warning "RAPL (Power Management): Not available"
        RAPL_SUPPORTED=0
    fi
    
    # Check Uncore Frequency Scaling
    if [[ -d /sys/devices/system/cpu/intel_uncore_frequency ]]; then
        print_success "Uncore Frequency Scaling: Available"
        UNCORE_SUPPORTED=1
    else
        print_warning "Uncore Frequency Scaling: Not available"
        UNCORE_SUPPORTED=0
    fi
    
    # Check CXL (Compute Express Link)
    if [[ -d /sys/bus/cxl ]]; then
        print_success "CXL (Compute Express Link): Available"
        CXL_SUPPORTED=1
    else
        print_warning "CXL (Compute Express Link): Not available"
        CXL_SUPPORTED=0
    fi
    
    # Check for Intel CPU specific features
    if grep -q "GenuineIntel" /proc/cpuinfo; then
        print_info "Intel CPU detected - full feature set may be available"
        INTEL_CPU=1
    else
        print_warning "Non-Intel CPU detected - some features may be limited"
        INTEL_CPU=0
    fi
    
    # Check prefetch control via MSR
    if [[ -c /dev/cpu/0/msr ]]; then
        print_success "MSR access available for prefetch control"
        PREFETCH_SUPPORTED=1
    else
        print_warning "MSR access not available for prefetch control"
        PREFETCH_SUPPORTED=0
    fi
    
    # ------------------------------------------------------------------
    # Additional feature detection
    # ------------------------------------------------------------------
    echo
    print_info "Additional CPU Feature Flags:"

    check_cpu_flag "aes" "AES-NI (Hardware Encryption)"
    check_cpu_flag "avx" "AVX"
    check_cpu_flag "avx2" "AVX2"
    check_cpu_flag "avx512f" "AVX-512 Foundation"
    check_cpu_flag "vmx" "Virtualization (VT-x/VMX)"
    check_cpu_flag "svm" "Virtualization (AMD-V/SVM)"
    check_cpu_flag "sgx" "Software Guard Extensions (SGX)"
    check_cpu_flag "rtm" "TSX (RTM)"
    check_cpu_flag "hle" "TSX (HLE)"
    check_cpu_flag "sha_ni" "SHA Extensions"

    echo
    print_info "CPU Power & Frequency Features:"

    # Turbo Boost status (Intel specific)
    if [[ -f /sys/devices/system/cpu/intel_pstate/no_turbo ]]; then
        if [[ $(cat /sys/devices/system/cpu/intel_pstate/no_turbo) -eq 0 ]]; then
            print_success "Turbo Boost: Enabled"
        else
            print_warning "Turbo Boost: Disabled"
        fi
    fi

    # Scaling governor (first CPU as representative)
    if [[ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
        local gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
        echo "  Scaling Governor: $gov"
    fi

    # Microcode version
    if grep -q "microcode" /proc/cpuinfo; then
        local microcode=$(grep "microcode" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)
        echo "  Microcode Version: $microcode"
    fi
    
    echo
}

# Run individual test with timeout and error handling
run_test() {
    local test_name=$1
    local test_binary=$2
    local timeout_seconds=${3:-30}
    
    print_info "Running $test_name test..."
    ((TOTAL_TESTS++))
    
    if [[ ! -x "$test_binary" ]]; then
        print_error "$test_name: Binary not found or not executable: $test_binary"
        ((FAILED_TESTS++))
        return 1
    fi
    
    # Run test with timeout
    if timeout $timeout_seconds "$test_binary" > /tmp/test_output.log 2>&1; then
        print_success "$test_name: PASSED"
        ((PASSED_TESTS++))
        return 0
    else
        local exit_code=$?
        print_error "$test_name: FAILED (exit code: $exit_code)"
        
        # Show last few lines of output for debugging
        if [[ -f /tmp/test_output.log ]]; then
            echo "Last 5 lines of output:"
            tail -5 /tmp/test_output.log | sed 's/^/  /'
        fi
        
        ((FAILED_TESTS++))
        return 1
    fi
}

# Run all tests
run_all_tests() {
    print_header "Running Hardware Knobs Test Suite"
    
    # RDT Tests
    if [[ $RDT_SUPPORTED -eq 1 ]]; then
        print_info "Running RDT tests..."
        run_test "RDT Test" "build/rdt_test" 30
        run_test "RDT Monitor" "build/rdt_monitor" 10
    else
        print_warning "Skipping RDT tests (not supported)"
    fi
    
    # Prefetch Tests
    if [[ $PREFETCH_SUPPORTED -eq 1 ]]; then
        print_info "Running Prefetch tests..."
        run_test "Prefetch Test" "build/prefetch_test" 30
        run_test "Prefetch Benchmark" "build/prefetch_bench" 60
    else
        print_warning "Skipping Prefetch tests (MSR access not available)"
    fi
    
    # SMT Tests
    if [[ $SMT_SUPPORTED -eq 1 ]]; then
        print_info "Running SMT tests..."
        run_test "SMT Test" "build/smt_test" 30
        run_test "SMT Benchmark" "build/smt_bench" 60
    else
        print_warning "Skipping SMT tests (not supported)"
    fi
    
    # Uncore Tests
    if [[ $UNCORE_SUPPORTED -eq 1 ]]; then
        print_info "Running Uncore tests..."
        run_test "Uncore Test" "build/uncore_test" 30
    else
        print_warning "Skipping Uncore tests (not supported)"
    fi
    
    # RAPL Tests
    if [[ $RAPL_SUPPORTED -eq 1 ]]; then
        print_info "Running RAPL tests..."
        run_test "RAPL Test" "build/rapl_test" 30
    else
        print_warning "Skipping RAPL tests (not supported)"
    fi
    
    # CXL Tests
    if [[ $CXL_SUPPORTED -eq 1 ]]; then
        print_info "Running CXL tests..."
        run_test "CXL Test" "build/cxl_test" 30
    else
        print_warning "Skipping CXL tests (not supported)"
    fi
}

# Run specific test category
run_specific_test() {
    local category=$1
    
    case $category in
        "rdt")
            if [[ $RDT_SUPPORTED -eq 1 ]]; then
                run_test "RDT Test" "build/rdt_test" 30
                run_test "RDT Monitor" "build/rdt_monitor" 10
            else
                print_warning "RDT not supported on this system"
            fi
            ;;
        "prefetch")
            if [[ $PREFETCH_SUPPORTED -eq 1 ]]; then
                run_test "Prefetch Test" "build/prefetch_test" 30
                run_test "Prefetch Benchmark" "build/prefetch_bench" 60
            else
                print_warning "Prefetch control not available (MSR access required)"
            fi
            ;;
        "smt")
            if [[ $SMT_SUPPORTED -eq 1 ]]; then
                run_test "SMT Test" "build/smt_test" 30
                run_test "SMT Benchmark" "build/smt_bench" 60
            else
                print_warning "SMT not supported on this system"
            fi
            ;;
        "uncore")
            if [[ $UNCORE_SUPPORTED -eq 1 ]]; then
                run_test "Uncore Test" "build/uncore_test" 30
            else
                print_warning "Uncore frequency scaling not available"
            fi
            ;;
        "rapl")
            if [[ $RAPL_SUPPORTED -eq 1 ]]; then
                run_test "RAPL Test" "build/rapl_test" 30
            else
                print_warning "RAPL not available"
            fi
            ;;
        "cxl")
            if [[ $CXL_SUPPORTED -eq 1 ]]; then
                run_test "CXL Test" "build/cxl_test" 30
            else
                print_warning "CXL not available"
            fi
            ;;
        *)
            print_error "Unknown test category: $category"
            print_info "Available categories: rdt, prefetch, smt, uncore, rapl, cxl"
            exit 1
            ;;
    esac
}

# Show system information
show_system_info() {
    print_header "System Information"
    
    # OS Information
    print_info "Operating System:"
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        echo "  Distribution: $PRETTY_NAME"
    fi
    echo "  Kernel: $(uname -r)"
    echo "  Architecture: $(uname -m)"
    
    # Hardware information
    print_info "Hardware:"
    echo "  CPU Model: $(grep "model name" /proc/cpuinfo | head -1 | cut -d: -f2 | xargs)"
    echo "  CPU Cores: $(nproc)"
    echo "  Memory: $(free -h | grep "Mem:" | awk '{print $2}')"
    
    # Loaded modules
    print_info "Relevant Kernel Modules:"
    for module in msr cpuid intel_rapl intel_uncore; do
        if lsmod | grep -q "^$module "; then
            echo "  $module: loaded"
        else
            echo "  $module: not loaded"
        fi
    done
    
    echo
}

# Print final results
print_results() {
    echo
    print_header "Test Results Summary"
    
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    
    if [[ $FAILED_TESTS -eq 0 ]]; then
        print_success "All tests passed!"
        exit 0
    else
        print_error "Some tests failed"
        exit 1
    fi
}

# Show usage information
show_usage() {
    echo "Hardware Knobs Test Suite Runner"
    echo ""
    echo "Usage: $0 [OPTIONS] [TEST_CATEGORY]"
    echo ""
    echo "OPTIONS:"
    echo "  -h, --help      Show this help message"
    echo "  -i, --info      Show system information only"
    echo "  -f, --features  Show CPU feature detection only"
    echo "  -v, --verbose   Enable verbose output"
    echo ""
    echo "TEST_CATEGORY:"
    echo "  all             Run all tests (default)"
    echo "  rdt             Run RDT tests only"
    echo "  prefetch        Run prefetch tests only"
    echo "  smt             Run SMT tests only"
    echo "  uncore          Run uncore tests only"
    echo "  rapl            Run RAPL tests only"
    echo "  cxl             Run CXL tests only"
    echo ""
    echo "Examples:"
    echo "  sudo $0                    # Run all tests"
    echo "  sudo $0 rdt               # Run RDT tests only"
    echo "  sudo $0 --info            # Show system info only"
    echo "  sudo $0 --features        # Show CPU features only"
    echo ""
    echo "Note: Root privileges are required for hardware access"
}

# Main script logic
main() {
    detect_cpu_features
    exit 0
}

# Initialize variables
RDT_SUPPORTED=0
SMT_SUPPORTED=0
RAPL_SUPPORTED=0
UNCORE_SUPPORTED=0
CXL_SUPPORTED=0
PREFETCH_SUPPORTED=0
INTEL_CPU=0

# Run main function
main "$@"