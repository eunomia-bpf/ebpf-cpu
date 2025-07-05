#include "../common/common.h"
#include "../common/msr_utils.h"
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

// Benchmark parameters
#define BENCH_ARRAY_SIZE (32 * 1024 * 1024)  // 32MB per thread
#define BENCH_ITERATIONS 10
#define CACHE_LINE_SIZE 64
#define MAX_THREADS 16
#define BENCHMARK_DURATION 30  // seconds

// RDT benchmark types
typedef enum {
    BENCH_CACHE_INTENSIVE,
    BENCH_MEMORY_INTENSIVE,
    BENCH_MIXED_WORKLOAD,
    BENCH_POINTER_CHASE,
    BENCH_STREAM_COPY
} benchmark_type_t;

// Thread data structure
typedef struct {
    int thread_id;
    int clos_id;
    benchmark_type_t bench_type;
    void *data;
    size_t data_size;
    volatile int *running;
    uint64_t operations;
    uint64_t start_time;
    uint64_t end_time;
    double throughput;
    double latency;
} thread_data_t;

// RDT configuration structure
typedef struct {
    const char *name;
    uint64_t l3_mask;
    uint64_t mb_throttle;
    int num_threads;
    benchmark_type_t bench_type;
} rdt_config_t;

// Global variables
static volatile int g_running = 1;
static pthread_t threads[MAX_THREADS];
static thread_data_t thread_data[MAX_THREADS];

// Function declarations
void signal_handler(int sig);
int rdt_bench_init(void);
void rdt_bench_cleanup(void);
void* benchmark_thread(void *arg);
int setup_rdt_clos(int clos_id, uint64_t l3_mask, uint64_t mb_throttle);
int assign_thread_to_clos(int clos_id);
double benchmark_cache_intensive(void *data, size_t size, volatile int *running);
double benchmark_memory_intensive(void *data, size_t size, volatile int *running);
double benchmark_mixed_workload(void *data, size_t size, volatile int *running);
double benchmark_pointer_chase(void *data, size_t size, volatile int *running);
double benchmark_stream_copy(void *data, size_t size, volatile int *running);
void run_rdt_benchmark(const rdt_config_t *config);
void print_benchmark_results(const rdt_config_t *config, thread_data_t *results, int num_threads);
void monitor_rdt_metrics(int duration);

// Predefined benchmark configurations
static const rdt_config_t benchmark_configs[] = {
    {
        .name = "Baseline - No RDT Control",
        .l3_mask = 0xFFFF,  // All cache ways
        .mb_throttle = 0,   // No memory bandwidth throttling
        .num_threads = 4,
        .bench_type = BENCH_CACHE_INTENSIVE
    },
    {
        .name = "Cache Isolation - High Priority",
        .l3_mask = 0xFF00,  // Upper 8 ways
        .mb_throttle = 0,
        .num_threads = 2,
        .bench_type = BENCH_CACHE_INTENSIVE
    },
    {
        .name = "Cache Isolation - Low Priority",
        .l3_mask = 0x00FF,  // Lower 8 ways
        .mb_throttle = 0,
        .num_threads = 2,
        .bench_type = BENCH_CACHE_INTENSIVE
    },
    {
        .name = "Memory Bandwidth Throttling - 50%",
        .l3_mask = 0xFFFF,
        .mb_throttle = 50,
        .num_threads = 4,
        .bench_type = BENCH_MEMORY_INTENSIVE
    },
    {
        .name = "Memory Bandwidth Throttling - 25%",
        .l3_mask = 0xFFFF,
        .mb_throttle = 25,
        .num_threads = 4,
        .bench_type = BENCH_MEMORY_INTENSIVE
    },
    {
        .name = "Mixed Workload - Balanced",
        .l3_mask = 0xFFFF,
        .mb_throttle = 0,
        .num_threads = 8,
        .bench_type = BENCH_MIXED_WORKLOAD
    },
    {
        .name = "Pointer Chase - Cache Sensitive",
        .l3_mask = 0xF000,  // Only 4 ways
        .mb_throttle = 0,
        .num_threads = 2,
        .bench_type = BENCH_POINTER_CHASE
    },
    {
        .name = "Stream Copy - Bandwidth Sensitive",
        .l3_mask = 0xFFFF,
        .mb_throttle = 75,
        .num_threads = 4,
        .bench_type = BENCH_STREAM_COPY
    }
};

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting Comprehensive RDT Benchmark Suite");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Initialize RDT benchmark
    if (rdt_bench_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize RDT benchmark");
        return EXIT_FAILURE;
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    int config_index = -1;
    if (argc > 1) {
        config_index = atoi(argv[1]);
        if (config_index < 0 || config_index >= (int)(sizeof(benchmark_configs) / sizeof(benchmark_configs[0]))) {
            PRINT_ERROR("Invalid configuration index: %d", config_index);
            config_index = -1;
        }
    }
    
    if (config_index == -1) {
        // Run all benchmark configurations
        PRINT_INFO("Running all RDT benchmark configurations...");
        for (size_t i = 0; i < sizeof(benchmark_configs) / sizeof(benchmark_configs[0]); i++) {
            if (!g_running) break;
            
            PRINT_INFO("=== Configuration %zu: %s ===", i, benchmark_configs[i].name);
            run_rdt_benchmark(&benchmark_configs[i]);
            
            // Wait between configurations
            if (i < sizeof(benchmark_configs) / sizeof(benchmark_configs[0]) - 1) {
                PRINT_INFO("Waiting 5 seconds before next configuration...");
                sleep(5);
            }
        }
    } else {
        // Run specific configuration
        PRINT_INFO("Running configuration %d: %s", config_index, benchmark_configs[config_index].name);
        run_rdt_benchmark(&benchmark_configs[config_index]);
    }
    
    // Start RDT monitoring in background
    PRINT_INFO("Starting RDT monitoring for comprehensive analysis...");
    monitor_rdt_metrics(10);
    
    rdt_bench_cleanup();
    
    PRINT_INFO("RDT benchmark suite completed");
    return EXIT_SUCCESS;
}

int rdt_bench_init(void) {
    // Check RDT support
    if (check_cpu_feature("rdt_a") != SUCCESS) {
        PRINT_ERROR("RDT not supported on this CPU");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check MSR availability
    if (msr_check_available() != SUCCESS) {
        PRINT_ERROR("MSR access not available");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Initialize default CLOS configurations
    // CLOS 0: Default (all resources)
    setup_rdt_clos(0, 0xFFFF, 0);
    
    PRINT_INFO("RDT benchmark initialized");
    return SUCCESS;
}

void rdt_bench_cleanup(void) {
    // Reset all threads to default CLOS
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        uint64_t value;
        if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) == SUCCESS) {
            value = value & 0x00000000FFFFFFFFULL;  // Clear CLOS bits
            msr_write_cpu(cpu, MSR_IA32_PQR_ASSOC, value);
        }
    }
    
    // Reset CLOS configurations to default
    for (int clos = 0; clos < 16; clos++) {
        msr_write_cpu(0, MSR_IA32_L3_MASK_0 + clos, 0xFFFF);
        if (check_cpu_feature("mba") == SUCCESS) {
            msr_write_cpu(0, MSR_IA32_MBA_THRTL_MSR + clos, 0);
        }
    }
    
    PRINT_INFO("RDT benchmark cleanup completed");
}

int setup_rdt_clos(int clos_id, uint64_t l3_mask, uint64_t mb_throttle) {
    // Set L3 cache allocation mask
    if (msr_write_cpu(0, MSR_IA32_L3_MASK_0 + clos_id, l3_mask) != SUCCESS) {
        PRINT_ERROR("Failed to set L3 mask for CLOS %d", clos_id);
        return ERROR_SYSTEM;
    }
    
    // Set memory bandwidth throttling (if supported)
    if (check_cpu_feature("mba") == SUCCESS && mb_throttle > 0) {
        if (msr_write_cpu(0, MSR_IA32_MBA_THRTL_MSR + clos_id, mb_throttle) != SUCCESS) {
            PRINT_INFO("Memory bandwidth throttling not supported or failed for CLOS %d", clos_id);
        }
    }
    
    return SUCCESS;
}

int assign_thread_to_clos(int clos_id) {
    int cpu = sched_getcpu();
    if (cpu < 0) {
        return ERROR_SYSTEM;
    }
    
    uint64_t value;
    if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Set CLOS ID in bits 63:32
    value = (value & 0x00000000FFFFFFFFULL) | ((uint64_t)clos_id << 32);
    
    return msr_write_cpu(cpu, MSR_IA32_PQR_ASSOC, value);
}

void run_rdt_benchmark(const rdt_config_t *config) {
    // Setup RDT configuration
    if (setup_rdt_clos(1, config->l3_mask, config->mb_throttle) != SUCCESS) {
        PRINT_ERROR("Failed to setup RDT configuration");
        return;
    }
    
    // Initialize thread data
    for (int i = 0; i < config->num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].clos_id = 1;  // Use CLOS 1 for benchmark
        thread_data[i].bench_type = config->bench_type;
        thread_data[i].data_size = BENCH_ARRAY_SIZE;
        thread_data[i].running = &g_running;
        thread_data[i].operations = 0;
        thread_data[i].throughput = 0.0;
        thread_data[i].latency = 0.0;
        
        // Allocate data for each thread
        thread_data[i].data = malloc(thread_data[i].data_size);
        if (!thread_data[i].data) {
            PRINT_ERROR("Failed to allocate data for thread %d", i);
            return;
        }
        
        // Initialize data
        memset(thread_data[i].data, 0x55, thread_data[i].data_size);
    }
    
    // Create and start threads
    for (int i = 0; i < config->num_threads; i++) {
        if (pthread_create(&threads[i], NULL, benchmark_thread, &thread_data[i]) != 0) {
            PRINT_ERROR("Failed to create thread %d", i);
            return;
        }
    }
    
    // Let benchmark run for specified duration
    sleep(BENCHMARK_DURATION);
    
    // Stop benchmark
    g_running = 0;
    
    // Wait for all threads to complete
    for (int i = 0; i < config->num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Print results
    print_benchmark_results(config, thread_data, config->num_threads);
    
    // Cleanup thread data
    for (int i = 0; i < config->num_threads; i++) {
        free(thread_data[i].data);
    }
    
    // Reset running flag
    g_running = 1;
}

void* benchmark_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    
    // Assign thread to CLOS
    if (assign_thread_to_clos(data->clos_id) != SUCCESS) {
        PRINT_ERROR("Failed to assign thread %d to CLOS %d", data->thread_id, data->clos_id);
        return NULL;
    }
    
    // Run benchmark based on type
    data->start_time = get_timestamp_us();
    
    switch (data->bench_type) {
        case BENCH_CACHE_INTENSIVE:
            data->throughput = benchmark_cache_intensive(data->data, data->data_size, data->running);
            break;
        case BENCH_MEMORY_INTENSIVE:
            data->throughput = benchmark_memory_intensive(data->data, data->data_size, data->running);
            break;
        case BENCH_MIXED_WORKLOAD:
            data->throughput = benchmark_mixed_workload(data->data, data->data_size, data->running);
            break;
        case BENCH_POINTER_CHASE:
            data->throughput = benchmark_pointer_chase(data->data, data->data_size, data->running);
            break;
        case BENCH_STREAM_COPY:
            data->throughput = benchmark_stream_copy(data->data, data->data_size, data->running);
            break;
    }
    
    data->end_time = get_timestamp_us();
    data->latency = (data->end_time - data->start_time) / 1000.0; // Convert to ms
    
    return NULL;
}

double benchmark_cache_intensive(void *data, size_t size, volatile int *running) {
    volatile int *array = (volatile int *)data;
    size_t num_ints = size / sizeof(int);
    uint64_t operations = 0;
    
    // Cache-intensive workload: frequent random access to small data set
    size_t working_set = num_ints / 16;  // Use 1/16 of array for high cache locality
    
    while (*running) {
        for (size_t i = 0; i < working_set && *running; i++) {
            size_t index = (rand() % working_set);
            array[index] += array[(index + 1) % working_set];
            operations++;
        }
    }
    
    return (double)operations / 1000000.0;  // Return millions of operations per second
}

double benchmark_memory_intensive(void *data, size_t size, volatile int *running) {
    volatile char *array = (volatile char *)data;
    uint64_t bytes_processed = 0;
    
    // Memory-intensive workload: sequential access to large data set
    while (*running) {
        for (size_t i = 0; i < size && *running; i += CACHE_LINE_SIZE) {
            // Read and write to cause memory traffic
            array[i] = array[i] + 1;
            bytes_processed += CACHE_LINE_SIZE;
        }
    }
    
    return (double)bytes_processed / (1024 * 1024);  // Return MB/s
}

double benchmark_mixed_workload(void *data, size_t size, volatile int *running) {
    volatile int *array = (volatile int *)data;
    size_t num_ints = size / sizeof(int);
    uint64_t operations = 0;
    
    // Mixed workload: combination of cache-intensive and memory-intensive operations
    while (*running) {
        // Cache-intensive phase
        for (int i = 0; i < 1000 && *running; i++) {
            size_t index = rand() % (num_ints / 32);
            array[index] += array[(index + 1) % (num_ints / 32)];
            operations++;
        }
        
        // Memory-intensive phase
        for (size_t i = 0; i < num_ints && *running; i += 1024) {
            array[i] = array[i] + 1;
            operations++;
        }
    }
    
    return (double)operations / 1000000.0;
}

double benchmark_pointer_chase(void *data, size_t size, volatile int *running) {
    typedef struct node {
        struct node *next;
        char padding[CACHE_LINE_SIZE - sizeof(struct node *)];
    } node_t;
    
    node_t *nodes = (node_t *)data;
    size_t num_nodes = size / sizeof(node_t);
    uint64_t operations = 0;
    
    // Create a random pointer chain
    for (size_t i = 0; i < num_nodes - 1; i++) {
        nodes[i].next = &nodes[i + 1];
    }
    nodes[num_nodes - 1].next = &nodes[0];
    
    // Shuffle the chain
    for (size_t i = 0; i < num_nodes; i++) {
        size_t j = rand() % num_nodes;
        node_t *temp = nodes[i].next;
        nodes[i].next = nodes[j].next;
        nodes[j].next = temp;
    }
    
    // Perform pointer chase
    node_t *current = &nodes[0];
    while (*running) {
        for (size_t i = 0; i < num_nodes && *running; i++) {
            current = current->next;
            operations++;
        }
    }
    
    return (double)operations / 1000000.0;
}

double benchmark_stream_copy(void *data, size_t size, volatile int *running) {
    char *src = (char *)data;
    char *dst = (char *)malloc(size);
    if (!dst) return 0.0;
    
    char *original_dst = dst;  // Keep track of original malloc'd pointer
    uint64_t bytes_copied = 0;
    
    // Stream copy workload: bandwidth-intensive
    while (*running) {
        memcpy(dst, src, size);
        bytes_copied += size;
        
        // Swap src and dst to keep working
        char *temp = src;
        src = dst;
        dst = temp;
    }
    
    // Free the originally allocated memory
    if (original_dst == src) {
        free(src);
    } else {
        free(original_dst);
    }
    
    return (double)bytes_copied / (1024 * 1024);  // Return MB/s
}

void print_benchmark_results(const rdt_config_t *config, thread_data_t *results, int num_threads) {
    printf("\n=== Benchmark Results: %s ===\n", config->name);
    printf("L3 Cache Mask: 0x%04lX\n", config->l3_mask);
    printf("Memory Bandwidth Throttle: %lu%%\n", config->mb_throttle);
    printf("Number of Threads: %d\n", num_threads);
    printf("Benchmark Type: %s\n", 
           (config->bench_type == BENCH_CACHE_INTENSIVE) ? "Cache Intensive" :
           (config->bench_type == BENCH_MEMORY_INTENSIVE) ? "Memory Intensive" :
           (config->bench_type == BENCH_MIXED_WORKLOAD) ? "Mixed Workload" :
           (config->bench_type == BENCH_POINTER_CHASE) ? "Pointer Chase" :
           (config->bench_type == BENCH_STREAM_COPY) ? "Stream Copy" : "Unknown");
    
    printf("\nPer-Thread Results:\n");
    printf("Thread  Throughput    Latency(ms)  Duration(s)\n");
    printf("------  ----------    -----------  -----------\n");
    
    double total_throughput = 0.0;
    double avg_latency = 0.0;
    
    for (int i = 0; i < num_threads; i++) {
        double duration = (results[i].end_time - results[i].start_time) / 1000000.0;
        printf("%6d  %10.2f    %11.2f  %11.2f\n", 
               i, results[i].throughput, results[i].latency, duration);
        total_throughput += results[i].throughput;
        avg_latency += results[i].latency;
    }
    
    printf("------  ----------    -----------  -----------\n");
    printf("Total   %10.2f    %11.2f\n", total_throughput, avg_latency / num_threads);
    printf("\n");
}

void monitor_rdt_metrics(int duration) {
    PRINT_INFO("Monitoring RDT metrics for %d seconds...", duration);
    
    uint64_t start_time = get_timestamp_us();
    uint64_t end_time = start_time + (uint64_t)duration * 1000000;
    
    printf("Time(s)  LLC Occupancy(KB)  MBM Total(MB/s)  MBM Local(MB/s)\n");
    printf("-------  -----------------  ---------------  ---------------\n");
    
    uint64_t prev_mbm_total = 0, prev_mbm_local = 0;
    uint64_t prev_timestamp = start_time;
    
    while (get_timestamp_us() < end_time) {
        uint64_t llc_occupancy = 0, mbm_total = 0, mbm_local = 0;
        uint64_t curr_timestamp = get_timestamp_us();
        
        // Try to read RDT monitoring data
        // Note: This may show simulated data if hardware monitoring is not available
        uint64_t evtsel = 0 | (1ULL << 32);  // LLC occupancy
        if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) == SUCCESS) {
            if (msr_read_cpu(0, MSR_IA32_QM_CTR, &llc_occupancy) == SUCCESS) {
                llc_occupancy *= 64;  // Convert to bytes
            }
        }
        
        evtsel = 0 | (2ULL << 32);  // MBM total
        if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) == SUCCESS) {
            msr_read_cpu(0, MSR_IA32_QM_CTR, &mbm_total);
        }
        
        evtsel = 0 | (3ULL << 32);  // MBM local
        if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) == SUCCESS) {
            msr_read_cpu(0, MSR_IA32_QM_CTR, &mbm_local);
        }
        
        // Calculate rates
        if (prev_timestamp > 0) {
            uint64_t time_diff = curr_timestamp - prev_timestamp;
            uint64_t mbm_total_rate = 0, mbm_local_rate = 0;
            
            if (time_diff > 0) {
                mbm_total_rate = (mbm_total - prev_mbm_total) * 1000000 / time_diff;
                mbm_local_rate = (mbm_local - prev_mbm_local) * 1000000 / time_diff;
            }
            
            printf("%7.1f  %17lu  %15lu  %15lu\n",
                   (curr_timestamp - start_time) / 1000000.0,
                   llc_occupancy / 1024,
                   mbm_total_rate / (1024 * 1024),
                   mbm_local_rate / (1024 * 1024));
        }
        
        prev_mbm_total = mbm_total;
        prev_mbm_local = mbm_local;
        prev_timestamp = curr_timestamp;
        
        sleep(1);
    }
    
    printf("\nRDT monitoring completed.\n");
}

void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    PRINT_INFO("Received signal, stopping benchmark...");
}