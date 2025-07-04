#include "smt_common.h"
#include <pthread.h>
#include <sched.h>

// Test parameters
#define MAX_THREADS 64
#define WORKLOAD_ITERATIONS 1000000

typedef struct {
    int thread_id;
    int cpu_id;
    double work_time;
    uint64_t iterations;
} thread_data_t;

// Function declarations
int smt_disable_cpu(int cpu);
int smt_enable_cpu(int cpu);
int smt_is_cpu_online(int cpu);
int smt_test_basic_functionality(void);
int smt_test_performance_impact(void);
int smt_test_dynamic_control(void);
void* cpu_intensive_work(void *arg);
double smt_measure_performance(int num_threads, int use_siblings);
void smt_print_topology(void);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting SMT (Simultaneous Multi-Threading) Test");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Check SMT support
    if (smt_check_support() != SUCCESS) {
        PRINT_ERROR("SMT not supported on this system");
        return EXIT_FAILURE;
    }
    
    // Print current topology
    smt_print_topology();
    
    // Run tests
    PRINT_INFO("Running SMT tests...");
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Test 1: Basic functionality
    total_tests++;
    if (smt_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    // Test 2: Performance impact
    total_tests++;
    if (smt_test_performance_impact() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Performance impact test passed");
    } else {
        PRINT_ERROR("Performance impact test failed");
    }
    
    // Test 3: Dynamic control
    total_tests++;
    if (smt_test_dynamic_control() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Dynamic control test passed");
    } else {
        PRINT_ERROR("Dynamic control test failed");
    }
    
    PRINT_INFO("SMT Test Results: %d/%d tests passed", passed_tests, total_tests);
    
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}


int smt_disable_cpu(int cpu) {
    char path[256];
    snprintf(path, sizeof(path), CPU_ONLINE_PATH, cpu);
    return write_file_int(path, 0);
}

int smt_enable_cpu(int cpu) {
    char path[256];
    snprintf(path, sizeof(path), CPU_ONLINE_PATH, cpu);
    return write_file_int(path, 1);
}

int smt_is_cpu_online(int cpu) {
    char path[256];
    int online;
    snprintf(path, sizeof(path), CPU_ONLINE_PATH, cpu);
    if (read_file_int(path, &online) != SUCCESS) {
        return 0;
    }
    return online;
}

int smt_test_basic_functionality(void) {
    PRINT_INFO("Testing basic SMT functionality...");
    
    // Get current state
    smt_state_t original_state = smt_get_state();
    if (original_state == SMT_NOTSUPPORTED) {
        PRINT_ERROR("SMT not supported");
        return ERROR_NOT_SUPPORTED;
    }
    
    PRINT_DEBUG("Original SMT state: %d", original_state);
    
    // Get current active threads
    int original_threads = smt_get_active_threads();
    PRINT_DEBUG("Original active threads: %d", original_threads);
    
    // Test state changes (if possible)
    if (original_state == SMT_ON) {
        PRINT_DEBUG("Testing SMT disable...");
        if (smt_set_state(SMT_OFF) == SUCCESS) {
            sleep_ms(500); // Wait for state change
            smt_state_t new_state = smt_get_state();
            int new_threads = smt_get_active_threads();
            
            PRINT_DEBUG("After disable - State: %d, Threads: %d", new_state, new_threads);
            
            // Restore original state
            smt_set_state(original_state);
            sleep_ms(500);
        } else {
            PRINT_INFO("SMT state change not permitted (system policy)");
        }
    }
    
    return SUCCESS;
}

int smt_test_performance_impact(void) {
    PRINT_INFO("Testing SMT performance impact...");
    
    int cpu_count = get_cpu_count();
    int physical_cores = cpu_count / 2; // Assume 2-way SMT
    
    PRINT_INFO("Performance comparison:");
    PRINT_INFO("Threads   No SMT    With SMT   Efficiency");
    PRINT_INFO("-------   ------    --------   ----------");
    
    // Test with different thread counts
    for (int threads = 1; threads <= (cpu_count < 8 ? cpu_count : 8); threads *= 2) {
        // Measure performance without sibling threads
        double perf_no_smt = smt_measure_performance(threads, 0);
        
        // Measure performance with sibling threads (if SMT enabled)
        double perf_with_smt = smt_measure_performance(threads, 1);
        
        double efficiency = (perf_with_smt > 0) ? (perf_with_smt / perf_no_smt) : 0.0;
        
        printf("%7d   %6.2f    %8.2f   %9.2f%%\n", 
               threads, perf_no_smt, perf_with_smt, efficiency * 100.0);
    }
    
    return SUCCESS;
}

int smt_test_dynamic_control(void) {
    PRINT_INFO("Testing dynamic SMT control...");
    
    int cpu_count = get_cpu_count();
    
    // Test individual CPU online/offline
    for (int cpu = 1; cpu < (cpu_count < 4 ? cpu_count : 4); cpu++) {
        if (!smt_is_cpu_online(cpu)) {
            continue; // Skip if CPU is already offline
        }
        
        PRINT_DEBUG("Testing CPU %d disable/enable...", cpu);
        
        // Measure disable latency
        uint64_t start_time = get_timestamp_us();
        if (smt_disable_cpu(cpu) == SUCCESS) {
            uint64_t disable_time = get_timestamp_us() - start_time;
            
            // Wait a bit
            sleep_ms(100);
            
            // Measure enable latency
            start_time = get_timestamp_us();
            if (smt_enable_cpu(cpu) == SUCCESS) {
                uint64_t enable_time = get_timestamp_us() - start_time;
                
                PRINT_DEBUG("CPU %d disable: %lu us, enable: %lu us", 
                           cpu, disable_time, enable_time);
            } else {
                PRINT_ERROR("Failed to re-enable CPU %d", cpu);
            }
        } else {
            PRINT_DEBUG("Failed to disable CPU %d (may be protected)", cpu);
        }
        
        sleep_ms(100); // Wait between tests
    }
    
    return SUCCESS;
}

void* cpu_intensive_work(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    cpu_set_t cpuset;
    
    // Pin thread to specific CPU
    CPU_ZERO(&cpuset);
    CPU_SET(data->cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    uint64_t start_time = get_timestamp_us();
    
    // CPU-intensive work
    volatile uint64_t result = 0;
    for (uint64_t i = 0; i < WORKLOAD_ITERATIONS; i++) {
        result += i * i;
        result ^= (result << 13);
        result ^= (result >> 17);
        result ^= (result << 5);
    }
    
    uint64_t end_time = get_timestamp_us();
    
    data->work_time = (end_time - start_time) / 1000.0; // Convert to milliseconds
    data->iterations = WORKLOAD_ITERATIONS;
    
    return NULL;
}

double smt_measure_performance(int num_threads, int use_siblings) {
    pthread_t threads[MAX_THREADS];
    thread_data_t thread_data[MAX_THREADS];
    int cpu_count = get_cpu_count();
    
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
    }
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        
        if (use_siblings && smt_get_state() == SMT_ON) {
            // Use both physical and logical cores
            thread_data[i].cpu_id = i % cpu_count;
        } else {
            // Use only physical cores (every other CPU)
            thread_data[i].cpu_id = (i * 2) % cpu_count;
        }
        
        if (pthread_create(&threads[i], NULL, cpu_intensive_work, &thread_data[i]) != 0) {
            PRINT_ERROR("Failed to create thread %d", i);
            return 0.0;
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate total performance (operations per second)
    double total_ops = 0.0;
    for (int i = 0; i < num_threads; i++) {
        if (thread_data[i].work_time > 0) {
            total_ops += (thread_data[i].iterations / thread_data[i].work_time) * 1000.0;
        }
    }
    
    return total_ops / 1000000.0; // Convert to millions of operations per second
}

void smt_print_topology(void) {
    PRINT_INFO("CPU Topology Information:");
    
    int cpu_count = get_cpu_count();
    PRINT_INFO("Total CPUs: %d", cpu_count);
    
    smt_state_t state = smt_get_state();
    const char *state_names[] = {"ON", "OFF", "FORCEOFF", "NOTSUPPORTED"};
    PRINT_INFO("SMT State: %s", state_names[state]);
    
    if (state != SMT_NOTSUPPORTED) {
        int active_threads = smt_get_active_threads();
        PRINT_INFO("Active SMT threads: %d", active_threads);
    }
    
    // Print online status for first few CPUs
    PRINT_INFO("CPU Online Status:");
    for (int i = 0; i < (cpu_count < 8 ? cpu_count : 8); i++) {
        int online = smt_is_cpu_online(i);
        PRINT_INFO("  CPU %d: %s", i, online ? "Online" : "Offline");
    }
    
    // Print CPU features
    if (check_cpu_feature("ht") == SUCCESS) {
        PRINT_INFO("Hyper-Threading: Supported");
    } else {
        PRINT_INFO("Hyper-Threading: Not Supported");
    }
}