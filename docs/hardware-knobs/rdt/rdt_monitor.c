#include "../common/common.h"
#include "../common/msr_utils.h"
#include <signal.h>

// RDT monitoring definitions
#define MAX_RMID 256
#define MONITORING_INTERVAL_MS 100

static volatile int running = 1;

typedef struct {
    int rmid;
    uint64_t llc_occupancy;
    uint64_t mbm_total;
    uint64_t mbm_local;
    uint64_t timestamp;
} rdt_monitor_data_t;

// Function declarations
int rdt_monitor_init(void);
int rdt_monitor_cleanup(void);
int rdt_monitor_read_llc_occupancy(int rmid, uint64_t *occupancy);
int rdt_monitor_read_mbm_total(int rmid, uint64_t *bandwidth);
int rdt_monitor_read_mbm_local(int rmid, uint64_t *bandwidth);
int rdt_monitor_set_rmid(int cpu, int rmid);
int rdt_monitor_get_rmid(int cpu, int *rmid);
void rdt_monitor_print_data(rdt_monitor_data_t *data);
void rdt_monitor_continuous(int duration_seconds);
void signal_handler(int sig);

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting RDT Monitor");
    
    // Check permissions
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    // Initialize monitoring
    if (rdt_monitor_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize RDT monitoring");
        return EXIT_FAILURE;
    }
    
    // Set up signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Parse command line arguments
    int duration = 10; // Default 10 seconds
    if (argc > 1) {
        duration = atoi(argv[1]);
        if (duration <= 0) {
            PRINT_ERROR("Invalid duration: %s", argv[1]);
            return EXIT_FAILURE;
        }
    }
    
    PRINT_INFO("Starting continuous monitoring for %d seconds...", duration);
    PRINT_INFO("Press Ctrl+C to stop monitoring");
    
    // Start continuous monitoring
    rdt_monitor_continuous(duration);
    
    // Cleanup
    rdt_monitor_cleanup();
    
    PRINT_INFO("RDT monitoring completed");
    return EXIT_SUCCESS;
}

int rdt_monitor_init(void) {
    // Check if monitoring is supported
    if (check_cpu_feature("rdt_m") != SUCCESS) {
        PRINT_ERROR("RDT monitoring not supported on this CPU");
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check MSR availability
    if (msr_check_available() != SUCCESS) {
        return ERROR_NOT_SUPPORTED;
    }
    
    // Initialize monitoring MSRs
    // Set default RMID (0) for all CPUs
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        if (rdt_monitor_set_rmid(cpu, 0) != SUCCESS) {
            PRINT_ERROR("Failed to set RMID for CPU %d", cpu);
            return ERROR_SYSTEM;
        }
    }
    
    PRINT_INFO("RDT monitoring initialized successfully");
    return SUCCESS;
}

int rdt_monitor_cleanup(void) {
    // Reset all CPUs to default RMID (0)
    int cpu_count = get_cpu_count();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        rdt_monitor_set_rmid(cpu, 0);
    }
    
    PRINT_INFO("RDT monitoring cleanup completed");
    return SUCCESS;
}

int rdt_monitor_read_llc_occupancy(int rmid, uint64_t *occupancy) {
    if (occupancy == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    // Select LLC occupancy monitoring event
    uint64_t evtsel = rmid | (1ULL << 32); // Event ID 1 for LLC occupancy
    if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Read the counter
    if (msr_read_cpu(0, MSR_IA32_QM_CTR, occupancy) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Convert to bytes (scaling factor is typically 64 bytes per unit)
    *occupancy *= 64;
    
    return SUCCESS;
}

int rdt_monitor_read_mbm_total(int rmid, uint64_t *bandwidth) {
    if (bandwidth == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    // Select MBM total event
    uint64_t evtsel = rmid | (2ULL << 32); // Event ID 2 for MBM total
    if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Read the counter
    if (msr_read_cpu(0, MSR_IA32_QM_CTR, bandwidth) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    return SUCCESS;
}

int rdt_monitor_read_mbm_local(int rmid, uint64_t *bandwidth) {
    if (bandwidth == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    // Select MBM local event
    uint64_t evtsel = rmid | (3ULL << 32); // Event ID 3 for MBM local
    if (msr_write_cpu(0, MSR_IA32_QM_EVTSEL, evtsel) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Read the counter
    if (msr_read_cpu(0, MSR_IA32_QM_CTR, bandwidth) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    return SUCCESS;
}

int rdt_monitor_set_rmid(int cpu, int rmid) {
    if (rmid >= MAX_RMID) {
        return ERROR_INVALID_PARAM;
    }
    
    uint64_t value;
    if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Set RMID in bits 63:32
    value = (value & 0x00000000FFFFFFFFULL) | ((uint64_t)rmid << 32);
    
    return msr_write_cpu(cpu, MSR_IA32_PQR_ASSOC, value);
}

int rdt_monitor_get_rmid(int cpu, int *rmid) {
    if (rmid == NULL) {
        return ERROR_INVALID_PARAM;
    }
    
    uint64_t value;
    if (msr_read_cpu(cpu, MSR_IA32_PQR_ASSOC, &value) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    *rmid = (int)(value >> 32);
    return SUCCESS;
}

void rdt_monitor_print_data(rdt_monitor_data_t *data) {
    printf("RMID: %d, LLC: %8lu KB, MBM Total: %8lu MB/s, MBM Local: %8lu MB/s\n",
           data->rmid,
           data->llc_occupancy / 1024,
           data->mbm_total / (1024 * 1024),
           data->mbm_local / (1024 * 1024));
}

void rdt_monitor_continuous(int duration_seconds) {
    uint64_t start_time = get_timestamp_us();
    uint64_t end_time = start_time + (uint64_t)duration_seconds * 1000000;
    
    rdt_monitor_data_t prev_data = {0};
    rdt_monitor_data_t curr_data = {0};
    
    PRINT_INFO("Time    RMID  LLC Occupancy  MBM Total    MBM Local");
    PRINT_INFO("-----  ----  -------------  ----------   ----------");
    
    while (running && get_timestamp_us() < end_time) {
        curr_data.timestamp = get_timestamp_us();
        curr_data.rmid = 0; // Monitor default RMID
        
        // Read monitoring data
        if (rdt_monitor_read_llc_occupancy(curr_data.rmid, &curr_data.llc_occupancy) != SUCCESS) {
            PRINT_ERROR("Failed to read LLC occupancy");
            break;
        }
        
        if (rdt_monitor_read_mbm_total(curr_data.rmid, &curr_data.mbm_total) != SUCCESS) {
            PRINT_ERROR("Failed to read MBM total");
            break;
        }
        
        if (rdt_monitor_read_mbm_local(curr_data.rmid, &curr_data.mbm_local) != SUCCESS) {
            PRINT_ERROR("Failed to read MBM local");
            break;
        }
        
        // Calculate bandwidth (if we have previous data)
        if (prev_data.timestamp > 0) {
            uint64_t time_diff = curr_data.timestamp - prev_data.timestamp;
            if (time_diff > 0) {
                uint64_t mbm_total_rate = (curr_data.mbm_total - prev_data.mbm_total) * 1000000 / time_diff;
                uint64_t mbm_local_rate = (curr_data.mbm_local - prev_data.mbm_local) * 1000000 / time_diff;
                
                printf("%5.1f  %4d  %8lu KB    %8lu MB/s  %8lu MB/s\n",
                       (curr_data.timestamp - start_time) / 1000000.0,
                       curr_data.rmid,
                       curr_data.llc_occupancy / 1024,
                       mbm_total_rate / (1024 * 1024),
                       mbm_local_rate / (1024 * 1024));
            }
        }
        
        prev_data = curr_data;
        
        // Sleep for monitoring interval
        sleep_ms(MONITORING_INTERVAL_MS);
    }
}

void signal_handler(int sig) {
    (void)sig; // Suppress unused parameter warning
    running = 0;
    PRINT_INFO("Received signal, stopping monitoring...");
}