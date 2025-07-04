#include "prefetch_common.h"
#include <signal.h>

// Benchmark parameters
#define BENCH_ARRAY_SIZE (64 * 1024 * 1024)  // 64MB
#define BENCH_ITERATIONS 5
#define CACHE_LINE_SIZE 64

static volatile int running = 1;

typedef struct {
    const char *name;
    double bandwidth;
    double latency;
    uint64_t cache_misses;
} benchmark_result_t;

// Function declarations
int prefetch_benchmark_init(void);
void prefetch_benchmark_cleanup(void);
double benchmark_sequential_read(void *data, size_t size);
double benchmark_sequential_write(void *data, size_t size);
double benchmark_random_read(void *data, size_t size);
double benchmark_stride_read(void *data, size_t size, int stride);
double benchmark_pointer_chase(void *data, size_t size);
void benchmark_with_prefetch_config(uint64_t config, const char *config_name);
void signal_handler(int sig);
void print_benchmark_header(void);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting Hardware Prefetch Benchmark");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Initialize benchmark
    if (prefetch_benchmark_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize prefetch benchmark");
        return EXIT_FAILURE;
    }
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Save original prefetch configuration
    uint64_t original_config;
    if (prefetch_read_config(&original_config) != SUCCESS) {
        PRINT_ERROR("Failed to read original prefetch configuration");
        return EXIT_FAILURE;
    }
    
    print_benchmark_header();
    
    // Benchmark different prefetch configurations
    benchmark_with_prefetch_config(0x0, "ALL_ENABLED");
    benchmark_with_prefetch_config(0x1, "L2_HW_DISABLED");
    benchmark_with_prefetch_config(0x2, "L2_ADJ_DISABLED");
    benchmark_with_prefetch_config(0x4, "DCU_STREAM_DISABLED");
    benchmark_with_prefetch_config(0x8, "DCU_IP_DISABLED");
    benchmark_with_prefetch_config(0xF, "ALL_DISABLED");
    
    // Restore original configuration
    prefetch_write_config(original_config);
    
    prefetch_benchmark_cleanup();
    
    PRINT_INFO("Prefetch benchmark completed");
    return EXIT_SUCCESS;
}

int prefetch_benchmark_init(void) {
    // Check prefetch support
    if (prefetch_check_support() != SUCCESS) {
        return ERROR_NOT_SUPPORTED;
    }
    
    PRINT_INFO("Prefetch benchmark initialized");
    return SUCCESS;
}

void prefetch_benchmark_cleanup(void) {
    PRINT_INFO("Prefetch benchmark cleanup completed");
}

double benchmark_sequential_read(void *data, size_t size) {
    volatile char *ptr = (volatile char *)data;
    uint64_t start_time, end_time;
    volatile char dummy = 0;
    
    // Clear CPU caches (rough approximation)
    for (size_t i = 0; i < size * 2; i += CACHE_LINE_SIZE) {
        dummy += ptr[i % size];
    }
    
    start_time = get_timestamp_us();
    
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
            dummy += ptr[i];
        }
    }
    
    end_time = get_timestamp_us();
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_read = (double)size * BENCH_ITERATIONS;
    
    return (bytes_read / (1024 * 1024)) / time_sec;
}

double benchmark_sequential_write(void *data, size_t size) {
    volatile char *ptr = (volatile char *)data;
    uint64_t start_time, end_time;
    
    start_time = get_timestamp_us();
    
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
            ptr[i] = (char)(i & 0xFF);
        }
    }
    
    end_time = get_timestamp_us();
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_written = (double)size * BENCH_ITERATIONS;
    
    return (bytes_written / (1024 * 1024)) / time_sec;
}

double benchmark_random_read(void *data, size_t size) {
    volatile char *ptr = (volatile char *)data;
    uint64_t start_time, end_time;
    volatile char dummy = 0;
    
    // Generate random indices
    size_t num_accesses = size / (CACHE_LINE_SIZE * 16); // Reduce number for random access
    size_t *indices = malloc(num_accesses * sizeof(size_t));
    if (!indices) {
        return 0.0;
    }
    
    for (size_t i = 0; i < num_accesses; i++) {
        indices[i] = (rand() % (size / CACHE_LINE_SIZE)) * CACHE_LINE_SIZE;
    }
    
    start_time = get_timestamp_us();
    
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        for (size_t i = 0; i < num_accesses; i++) {
            dummy += ptr[indices[i]];
        }
    }
    
    end_time = get_timestamp_us();
    
    free(indices);
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_read = (double)num_accesses * CACHE_LINE_SIZE * BENCH_ITERATIONS;
    
    return (bytes_read / (1024 * 1024)) / time_sec;
}

double benchmark_stride_read(void *data, size_t size, int stride) {
    volatile char *ptr = (volatile char *)data;
    uint64_t start_time, end_time;
    volatile char dummy = 0;
    
    size_t stride_bytes = stride * CACHE_LINE_SIZE;
    
    start_time = get_timestamp_us();
    
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        for (size_t i = 0; i < size; i += stride_bytes) {
            dummy += ptr[i];
        }
    }
    
    end_time = get_timestamp_us();
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_read = (double)(size / stride_bytes) * CACHE_LINE_SIZE * BENCH_ITERATIONS;
    
    return (bytes_read / (1024 * 1024)) / time_sec;
}

double benchmark_pointer_chase(void *data, size_t size) {
    struct node {
        struct node *next;
        char padding[CACHE_LINE_SIZE - sizeof(struct node *)];
    };
    
    struct node *nodes = (struct node *)data;
    size_t num_nodes = size / sizeof(struct node);
    uint64_t start_time, end_time;
    
    // Create a random linked list
    for (size_t i = 0; i < num_nodes - 1; i++) {
        nodes[i].next = &nodes[i + 1];
    }
    nodes[num_nodes - 1].next = &nodes[0];
    
    // Shuffle the list to make it more random
    for (size_t i = 0; i < num_nodes; i++) {
        size_t j = rand() % num_nodes;
        struct node *temp = nodes[i].next;
        nodes[i].next = nodes[j].next;
        nodes[j].next = temp;
    }
    
    start_time = get_timestamp_us();
    
    struct node *current = &nodes[0];
    for (int iter = 0; iter < BENCH_ITERATIONS; iter++) {
        for (size_t i = 0; i < num_nodes; i++) {
            current = current->next;
        }
    }
    
    end_time = get_timestamp_us();
    
    double time_sec = (end_time - start_time) / 1000000.0;
    double bytes_accessed = (double)num_nodes * sizeof(struct node) * BENCH_ITERATIONS;
    
    return (bytes_accessed / (1024 * 1024)) / time_sec;
}

void benchmark_with_prefetch_config(uint64_t config, const char *config_name) {
    if (!running) return;
    
    // Apply configuration
    if (prefetch_write_config(config) != SUCCESS) {
        PRINT_ERROR("Failed to apply configuration %s", config_name);
        return;
    }
    
    // Wait for configuration to take effect
    sleep_ms(100);
    
    // Allocate test data
    void *data = malloc(BENCH_ARRAY_SIZE);
    if (!data) {
        PRINT_ERROR("Failed to allocate benchmark data");
        return;
    }
    
    // Initialize data
    memset(data, 0x55, BENCH_ARRAY_SIZE);
    
    // Run benchmarks
    double seq_read = benchmark_sequential_read(data, BENCH_ARRAY_SIZE);
    double seq_write = benchmark_sequential_write(data, BENCH_ARRAY_SIZE);
    double rand_read = benchmark_random_read(data, BENCH_ARRAY_SIZE);
    double stride2_read = benchmark_stride_read(data, BENCH_ARRAY_SIZE, 2);
    double stride8_read = benchmark_stride_read(data, BENCH_ARRAY_SIZE, 8);
    double pointer_chase = benchmark_pointer_chase(data, BENCH_ARRAY_SIZE / 2);
    
    printf("%-16s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f\n",
           config_name, seq_read, seq_write, rand_read, 
           stride2_read, stride8_read, pointer_chase);
    
    free(data);
}

void print_benchmark_header(void) {
    PRINT_INFO("Prefetch Configuration Performance Comparison (MB/s):");
    printf("Configuration    Seq Read Seq Writ Rand Rd  Stride2  Stride8  PtrChase\n");
    printf("---------------- -------- -------- -------- -------- -------- --------\n");
}

void signal_handler(int sig) {
    (void)sig;
    running = 0;
    PRINT_INFO("Received signal, stopping benchmark...");
}