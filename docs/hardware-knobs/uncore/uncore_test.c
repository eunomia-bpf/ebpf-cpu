#include "../common/common.h"
#include "../common/msr_utils.h"

// Uncore frequency control definitions
#define UNCORE_FREQ_SYSFS_PATH "/sys/devices/system/cpu/intel_uncore_frequency"
#define MAX_DOMAINS 8

typedef struct {
    int domain_id;
    int min_freq_khz;
    int max_freq_khz;
    int current_freq_khz;
    int initial_min_khz;
    int initial_max_khz;
} uncore_domain_t;

// Function declarations
int uncore_check_support(void);
int uncore_init(void);
int uncore_cleanup(void);
int uncore_get_domains(uncore_domain_t *domains, int max_domains);
int uncore_set_min_freq(int domain, int freq_khz);
int uncore_set_max_freq(int domain, int freq_khz);
int uncore_get_current_freq(int domain, int *freq_khz);
int uncore_test_basic_functionality(void);
int uncore_test_frequency_scaling(void);
int uncore_test_performance_impact(void);
void uncore_print_info(void);

static uncore_domain_t domains[MAX_DOMAINS];
static int num_domains = 0;

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting Uncore Frequency Control Test");
    
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    if (uncore_check_support() != SUCCESS) {
        PRINT_ERROR("Uncore frequency control not supported");
        return EXIT_FAILURE;
    }
    
    if (uncore_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize uncore control");
        return EXIT_FAILURE;
    }
    
    uncore_print_info();
    
    int total_tests = 0;
    int passed_tests = 0;
    
    total_tests++;
    if (uncore_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    total_tests++;
    if (uncore_test_frequency_scaling() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Frequency scaling test passed");
    } else {
        PRINT_ERROR("Frequency scaling test failed");
    }
    
    total_tests++;
    if (uncore_test_performance_impact() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Performance impact test passed");
    } else {
        PRINT_ERROR("Performance impact test failed");
    }
    
    uncore_cleanup();
    
    PRINT_INFO("Uncore Test Results: %d/%d tests passed", passed_tests, total_tests);
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int uncore_check_support(void) {
    if (check_file_exists(UNCORE_FREQ_SYSFS_PATH) != SUCCESS) {
        PRINT_ERROR("Intel uncore frequency sysfs not found");
        return ERROR_NOT_SUPPORTED;
    }
    
    char vendor[64];
    if (get_cpu_vendor(vendor, sizeof(vendor)) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    if (strstr(vendor, "Intel") == NULL) {
        PRINT_ERROR("Uncore frequency control is Intel-specific");
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

int uncore_init(void) {
    num_domains = uncore_get_domains(domains, MAX_DOMAINS);
    if (num_domains <= 0) {
        PRINT_ERROR("No uncore domains found");
        return ERROR_SYSTEM;
    }
    
    PRINT_INFO("Found %d uncore domains", num_domains);
    return SUCCESS;
}

int uncore_cleanup(void) {
    for (int i = 0; i < num_domains; i++) {
        uncore_set_min_freq(i, domains[i].initial_min_khz);
        uncore_set_max_freq(i, domains[i].initial_max_khz);
    }
    PRINT_INFO("Restored original uncore frequencies");
    return SUCCESS;
}

int uncore_get_domains(uncore_domain_t *domains, int max_domains) {
    int domain_count = 0;
    
    for (int i = 0; i < max_domains; i++) {
        char min_path[256], max_path[256], cur_path[256];
        snprintf(min_path, sizeof(min_path), 
                "%s/package_%02d_die_%02d/min_freq_khz", UNCORE_FREQ_SYSFS_PATH, i, 0);
        snprintf(max_path, sizeof(max_path), 
                "%s/package_%02d_die_%02d/max_freq_khz", UNCORE_FREQ_SYSFS_PATH, i, 0);
        snprintf(cur_path, sizeof(cur_path), 
                "%s/package_%02d_die_%02d/current_freq_khz", UNCORE_FREQ_SYSFS_PATH, i, 0);
        
        if (check_file_exists(min_path) == SUCCESS) {
            domains[domain_count].domain_id = i;
            
            read_file_int(min_path, &domains[domain_count].min_freq_khz);
            read_file_int(max_path, &domains[domain_count].max_freq_khz);
            
            domains[domain_count].initial_min_khz = domains[domain_count].min_freq_khz;
            domains[domain_count].initial_max_khz = domains[domain_count].max_freq_khz;
            
            if (check_file_exists(cur_path) == SUCCESS) {
                read_file_int(cur_path, &domains[domain_count].current_freq_khz);
            }
            
            domain_count++;
        }
    }
    
    return domain_count;
}

int uncore_set_min_freq(int domain, int freq_khz) {
    if (domain >= num_domains) return ERROR_INVALID_PARAM;
    
    char path[256];
    snprintf(path, sizeof(path), 
            "%s/package_%02d_die_%02d/min_freq_khz", 
            UNCORE_FREQ_SYSFS_PATH, domains[domain].domain_id, 0);
    
    return write_file_int(path, freq_khz);
}

int uncore_set_max_freq(int domain, int freq_khz) {
    if (domain >= num_domains) return ERROR_INVALID_PARAM;
    
    char path[256];
    snprintf(path, sizeof(path), 
            "%s/package_%02d_die_%02d/max_freq_khz", 
            UNCORE_FREQ_SYSFS_PATH, domains[domain].domain_id, 0);
    
    return write_file_int(path, freq_khz);
}

int uncore_get_current_freq(int domain, int *freq_khz) {
    if (domain >= num_domains || freq_khz == NULL) return ERROR_INVALID_PARAM;
    
    char path[256];
    snprintf(path, sizeof(path), 
            "%s/package_%02d_die_%02d/current_freq_khz", 
            UNCORE_FREQ_SYSFS_PATH, domains[domain].domain_id, 0);
    
    return read_file_int(path, freq_khz);
}

int uncore_test_basic_functionality(void) {
    PRINT_INFO("Testing basic uncore functionality...");
    
    for (int i = 0; i < num_domains; i++) {
        int current_freq;
        if (uncore_get_current_freq(i, &current_freq) == SUCCESS) {
            PRINT_DEBUG("Domain %d current frequency: %d kHz", i, current_freq);
        }
        
        PRINT_DEBUG("Domain %d frequency range: %d - %d kHz", 
                   i, domains[i].min_freq_khz, domains[i].max_freq_khz);
    }
    
    return SUCCESS;
}

int uncore_test_frequency_scaling(void) {
    PRINT_INFO("Testing uncore frequency scaling...");
    
    if (num_domains == 0) return ERROR_SYSTEM;
    
    int domain = 0;
    int original_min = domains[domain].min_freq_khz;
    int original_max = domains[domain].max_freq_khz;
    
    int test_freq = original_min + (original_max - original_min) / 2;
    
    PRINT_DEBUG("Testing frequency change to %d kHz", test_freq);
    
    if (uncore_set_max_freq(domain, test_freq) != SUCCESS) {
        PRINT_ERROR("Failed to set max frequency");
        return ERROR_SYSTEM;
    }
    
    sleep_ms(100);
    
    int current_freq;
    if (uncore_get_current_freq(domain, &current_freq) == SUCCESS) {
        PRINT_DEBUG("Current frequency after change: %d kHz", current_freq);
    }
    
    uncore_set_max_freq(domain, original_max);
    
    return SUCCESS;
}

int uncore_test_performance_impact(void) {
    PRINT_INFO("Testing uncore frequency performance impact...");
    
    if (num_domains == 0) return ERROR_SYSTEM;
    
    const size_t test_size = 32 * 1024 * 1024; // 32MB
    void *buffer = malloc(test_size);
    if (!buffer) return ERROR_SYSTEM;
    
    memset(buffer, 0xAA, test_size);
    
    int domain = 0;
    int original_max = domains[domain].max_freq_khz;
    int low_freq = domains[domain].min_freq_khz;
    int high_freq = domains[domain].max_freq_khz;
    
    PRINT_INFO("Frequency    Memory BW (MB/s)    LLC Access Time");
    PRINT_INFO("---------    ----------------    ---------------");
    
    int test_freqs[] = {low_freq, (low_freq + high_freq) / 2, high_freq};
    const char* freq_names[] = {"Low", "Medium", "High"};
    
    for (int i = 0; i < 3; i++) {
        uncore_set_max_freq(domain, test_freqs[i]);
        sleep_ms(200);
        
        uint64_t start_time = get_timestamp_us();
        volatile char *ptr = (volatile char *)buffer;
        volatile char dummy = 0;
        
        for (size_t j = 0; j < test_size; j += 64) {
            dummy += ptr[j];
        }
        
        uint64_t end_time = get_timestamp_us();
        
        double time_sec = (end_time - start_time) / 1000000.0;
        double bandwidth = (test_size / (1024.0 * 1024.0)) / time_sec;
        double access_time = (time_sec * 1000000.0) / (test_size / 64);
        
        printf("%-9s    %16.1f    %15.2f ns\n", 
               freq_names[i], bandwidth, access_time);
    }
    
    uncore_set_max_freq(domain, original_max);
    free(buffer);
    
    return SUCCESS;
}

void uncore_print_info(void) {
    PRINT_INFO("Uncore Frequency Information:");
    PRINT_INFO("Domains found: %d", num_domains);
    
    for (int i = 0; i < num_domains; i++) {
        PRINT_INFO("Domain %d:", domains[i].domain_id);
        PRINT_INFO("  Min frequency: %d kHz", domains[i].min_freq_khz);
        PRINT_INFO("  Max frequency: %d kHz", domains[i].max_freq_khz);
        
        int current;
        if (uncore_get_current_freq(i, &current) == SUCCESS) {
            PRINT_INFO("  Current frequency: %d kHz", current);
        }
    }
}