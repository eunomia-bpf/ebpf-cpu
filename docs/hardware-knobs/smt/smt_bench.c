#include "smt_common.h"
#include <pthread.h>
#include <sched.h>

// Benchmark configuration
#define MAX_BENCHMARK_THREADS 32
#define BENCHMARK_DURATION_MS 1000
#define MEMORY_SIZE (4 * 1024 * 1024) // 4MB per thread

typedef enum {
    BENCH_CPU_INTENSIVE,
    BENCH_MEMORY_BOUND,
    BENCH_MIXED_WORKLOAD
} benchmark_type_t;

typedef struct {
    int thread_id;
    int cpu_id;
    benchmark_type_t bench_type;
    void *memory_buffer;
    volatile uint64_t operations;
    double execution_time_ms;
    int should_stop;
} benchmark_thread_data_t;

// Function declarations
void* benchmark_worker(void *arg);
double run_smt_benchmark(benchmark_type_t bench_type, int num_threads, int use_smt);
void cpu_intensive_benchmark(benchmark_thread_data_t *data);
void memory_bound_benchmark(benchmark_thread_data_t *data);
void mixed_workload_benchmark(benchmark_thread_data_t *data);
void print_benchmark_results(void);
int setup_cpu_affinity(int thread_id, int use_smt);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting SMT Performance Benchmark");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Check SMT support
    if (smt_check_support() != SUCCESS) {
        PRINT_ERROR("SMT not supported on this system");
        return EXIT_FAILURE;
    }
    
    print_benchmark_results();
    
    PRINT_INFO("SMT benchmark completed");
    return EXIT_SUCCESS;
}

void* benchmark_worker(void *arg) {
    benchmark_thread_data_t *data = (benchmark_thread_data_t *)arg;
    
    // Set CPU affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(data->cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    uint64_t start_time = get_timestamp_us();
    data->operations = 0;
    
    // Run benchmark based on type
    switch (data->bench_type) {
        case BENCH_CPU_INTENSIVE:
            cpu_intensive_benchmark(data);
            break;
        case BENCH_MEMORY_BOUND:
            memory_bound_benchmark(data);
            break;
        case BENCH_MIXED_WORKLOAD:
            mixed_workload_benchmark(data);
            break;
    }
    
    uint64_t end_time = get_timestamp_us();
    data->execution_time_ms = (end_time - start_time) / 1000.0;
    
    return NULL;
}

void cpu_intensive_benchmark(benchmark_thread_data_t *data) {
    volatile uint64_t result = 1;
    uint64_t end_time = get_timestamp_us() + (BENCHMARK_DURATION_MS * 1000);
    
    while (get_timestamp_us() < end_time && !data->should_stop) {
        // CPU-intensive calculations
        for (int i = 0; i < 1000; i++) {
            result *= 7;
            result ^= (result << 13);
            result ^= (result >> 17);
            result ^= (result << 5);
            result += 0x123456789ABCDEFULL;
        }
        data->operations += 1000;
    }
}

void memory_bound_benchmark(benchmark_thread_data_t *data) {
    volatile char *buffer = (volatile char *)data->memory_buffer;
    uint64_t end_time = get_timestamp_us() + (BENCHMARK_DURATION_MS * 1000);
    
    while (get_timestamp_us() < end_time && !data->should_stop) {
        // Memory-intensive operations
        for (int i = 0; i < MEMORY_SIZE; i += 64) {
            buffer[i] = (char)(i & 0xFF);
        }
        
        volatile char dummy = 0;
        for (int i = 0; i < MEMORY_SIZE; i += 64) {
            dummy += buffer[i];
        }
        
        data->operations += MEMORY_SIZE / 64;
    }
}

void mixed_workload_benchmark(benchmark_thread_data_t *data) {
    volatile char *buffer = (volatile char *)data->memory_buffer;
    volatile uint64_t cpu_result = 1;
    uint64_t end_time = get_timestamp_us() + (BENCHMARK_DURATION_MS * 1000);
    
    while (get_timestamp_us() < end_time && !data->should_stop) {
        // Mixed CPU and memory operations
        for (int i = 0; i < 100; i++) {
            // CPU work
            cpu_result *= 7;
            cpu_result ^= (cpu_result << 13);
            cpu_result ^= (cpu_result >> 17);
            
            // Memory work
            int mem_idx = (i * 64) % MEMORY_SIZE;
            buffer[mem_idx] = (char)(cpu_result & 0xFF);
            cpu_result += buffer[mem_idx];
        }
        data->operations += 100;
    }
}

double run_smt_benchmark(benchmark_type_t bench_type, int num_threads, int use_smt) {
    pthread_t threads[MAX_BENCHMARK_THREADS];
    benchmark_thread_data_t thread_data[MAX_BENCHMARK_THREADS];
    
    if (num_threads > MAX_BENCHMARK_THREADS) {
        num_threads = MAX_BENCHMARK_THREADS;
    }
    
    // Allocate memory buffers
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].memory_buffer = malloc(MEMORY_SIZE);
        if (!thread_data[i].memory_buffer) {
            PRINT_ERROR("Failed to allocate memory for thread %d", i);
            return 0.0;
        }
        memset(thread_data[i].memory_buffer, 0x55, MEMORY_SIZE);
    }
    
    // Create and run threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].bench_type = bench_type;
        thread_data[i].should_stop = 0;
        thread_data[i].cpu_id = setup_cpu_affinity(i, use_smt);
        
        if (pthread_create(&threads[i], NULL, benchmark_worker, &thread_data[i]) != 0) {
            PRINT_ERROR("Failed to create thread %d", i);
            return 0.0;
        }
    }
    
    // Wait for completion
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate total performance
    uint64_t total_operations = 0;
    double total_time = 0.0;
    
    for (int i = 0; i < num_threads; i++) {
        total_operations += thread_data[i].operations;
        if (thread_data[i].execution_time_ms > total_time) {
            total_time = thread_data[i].execution_time_ms;
        }
    }
    
    // Clean up memory
    for (int i = 0; i < num_threads; i++) {
        free(thread_data[i].memory_buffer);
    }
    
    // Return operations per second
    return (total_operations / total_time) * 1000.0;
}

int setup_cpu_affinity(int thread_id, int use_smt) {
    int cpu_count = get_cpu_count();
    
    if (use_smt) {
        // Use all available CPUs
        return thread_id % cpu_count;
    } else {
        // Use only even-numbered CPUs (physical cores)
        return (thread_id * 2) % cpu_count;
    }
}

void print_benchmark_results(void) {
    const char* bench_names[] = {"CPU Intensive", "Memory Bound", "Mixed Workload"};
    
    PRINT_INFO("SMT Performance Benchmark Results");
    PRINT_INFO("=================================");
    
    for (int bench_type = 0; bench_type < 3; bench_type++) {
        PRINT_INFO("\n%s Benchmark:", bench_names[bench_type]);
        PRINT_INFO("Threads  No SMT (Mops/s)  SMT (Mops/s)  SMT Efficiency  SMT Benefit");
        PRINT_INFO("-------  ----------------  -------------  --------------  -----------");
        
        for (int threads = 1; threads <= 8; threads *= 2) {
            // Run without SMT (physical cores only)
            double perf_no_smt = run_smt_benchmark((benchmark_type_t)bench_type, threads, 0);
            
            // Run with SMT if available
            double perf_with_smt = 0.0;
            if (smt_get_state() == SMT_ON) {
                perf_with_smt = run_smt_benchmark((benchmark_type_t)bench_type, threads, 1);
            }
            
            double efficiency = (perf_no_smt > 0) ? (perf_with_smt / perf_no_smt) : 0.0;
            double benefit = perf_with_smt - perf_no_smt;
            
            printf("%7d  %16.2f  %13.2f  %14.2f%%  %+10.2f\n",
                   threads, perf_no_smt / 1000000.0, perf_with_smt / 1000000.0,
                   efficiency * 100.0, benefit / 1000000.0);
        }
    }
    
    // Additional analysis
    PRINT_INFO("\nSMT Analysis:");
    
    // Check current SMT state
    smt_state_t smt_state = smt_get_state();
    const char *state_names[] = {"ON", "OFF", "FORCEOFF", "NOT SUPPORTED"};
    PRINT_INFO("Current SMT State: %s", state_names[smt_state]);
    
    if (smt_state == SMT_ON) {
        int active_threads = smt_get_active_threads();
        PRINT_INFO("Active SMT threads: %d", active_threads);
        
        // Measure SMT switching overhead
        uint64_t start_time = get_timestamp_us();
        for (int i = 0; i < 100; i++) {
            // Simulate rapid context switching
            sched_yield();
        }
        uint64_t end_time = get_timestamp_us();
        
        double switch_overhead = (end_time - start_time) / 100.0;
        PRINT_INFO("Context switch overhead: %.2f microseconds", switch_overhead);
    }
    
    // Memory bandwidth consideration
    PRINT_INFO("\nRecommendations:");
    PRINT_INFO("- CPU-intensive workloads: SMT may provide 20-30%% benefit");
    PRINT_INFO("- Memory-bound workloads: SMT benefit limited by memory bandwidth");
    PRINT_INFO("- Mixed workloads: SMT effectiveness depends on workload balance");
    PRINT_INFO("- For latency-sensitive apps: Consider disabling SMT to reduce jitter");
}