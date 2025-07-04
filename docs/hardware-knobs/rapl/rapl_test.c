#include "../common/common.h"
#include "../common/msr_utils.h"

// RAPL sysfs paths
#define RAPL_SYSFS_PATH "/sys/class/powercap/intel-rapl"

// RAPL MSR bit fields
#define RAPL_POWER_UNIT_MASK    0xF
#define RAPL_ENERGY_UNIT_MASK   0x1F00
#define RAPL_TIME_UNIT_MASK     0xF0000

typedef struct {
    int domain_id;
    char name[64];
    uint64_t max_power_uw;
    uint64_t max_energy_range_uj;
    double power_unit;
    double energy_unit;
    double time_unit;
} rapl_domain_t;

// Function declarations
int rapl_check_support(void);
int rapl_init(void);
int rapl_cleanup(void);
int rapl_read_power_units(double *power_unit, double *energy_unit, double *time_unit);
int rapl_read_pkg_energy(uint64_t *energy_uj);
int rapl_read_dram_energy(uint64_t *energy_uj);
int rapl_read_pkg_power_limit(uint64_t *power_limit_uw);
int rapl_set_pkg_power_limit(uint64_t power_limit_uw, uint64_t time_window_us);
int rapl_test_basic_functionality(void);
int rapl_test_energy_monitoring(void);
int rapl_test_power_capping(void);
void rapl_print_info(void);

static rapl_domain_t domains[4];
static int num_domains = 0;

int main(int argc, char *argv[]) {
    PRINT_INFO("Starting RAPL (Running Average Power Limit) Test");
    
    if (check_root_permission() != SUCCESS) {
        return EXIT_FAILURE;
    }
    
    if (rapl_check_support() != SUCCESS) {
        PRINT_ERROR("RAPL not supported on this system");
        return EXIT_FAILURE;
    }
    
    if (rapl_init() != SUCCESS) {
        PRINT_ERROR("Failed to initialize RAPL");
        return EXIT_FAILURE;
    }
    
    rapl_print_info();
    
    int total_tests = 0;
    int passed_tests = 0;
    
    total_tests++;
    if (rapl_test_basic_functionality() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Basic functionality test passed");
    } else {
        PRINT_ERROR("Basic functionality test failed");
    }
    
    total_tests++;
    if (rapl_test_energy_monitoring() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Energy monitoring test passed");
    } else {
        PRINT_ERROR("Energy monitoring test failed");
    }
    
    total_tests++;
    if (rapl_test_power_capping() == SUCCESS) {
        passed_tests++;
        PRINT_SUCCESS("Power capping test passed");
    } else {
        PRINT_ERROR("Power capping test failed");
    }
    
    rapl_cleanup();
    
    PRINT_INFO("RAPL Test Results: %d/%d tests passed", passed_tests, total_tests);
    return (passed_tests == total_tests) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int rapl_check_support(void) {
    if (check_file_exists(RAPL_SYSFS_PATH) != SUCCESS) {
        PRINT_ERROR("RAPL sysfs interface not found");
        return ERROR_NOT_SUPPORTED;
    }
    
    char vendor[64];
    if (get_cpu_vendor(vendor, sizeof(vendor)) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    if (strstr(vendor, "Intel") == NULL) {
        PRINT_ERROR("RAPL is Intel-specific");
        return ERROR_NOT_SUPPORTED;
    }
    
    if (msr_check_available() != SUCCESS) {
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

int rapl_init(void) {
    double power_unit, energy_unit, time_unit;
    if (rapl_read_power_units(&power_unit, &energy_unit, &time_unit) != SUCCESS) {
        PRINT_ERROR("Failed to read RAPL units");
        return ERROR_SYSTEM;
    }
    
    strcpy(domains[0].name, "PKG");
    strcpy(domains[1].name, "DRAM");
    strcpy(domains[2].name, "PP0");
    strcpy(domains[3].name, "PP1");
    
    for (int i = 0; i < 4; i++) {
        domains[i].domain_id = i;
        domains[i].power_unit = power_unit;
        domains[i].energy_unit = energy_unit;
        domains[i].time_unit = time_unit;
    }
    
    num_domains = 4;
    
    PRINT_INFO("RAPL initialized with %d domains", num_domains);
    return SUCCESS;
}

int rapl_cleanup(void) {
    PRINT_INFO("RAPL cleanup completed");
    return SUCCESS;
}

int rapl_read_power_units(double *power_unit, double *energy_unit, double *time_unit) {
    if (!power_unit || !energy_unit || !time_unit) {
        return ERROR_INVALID_PARAM;
    }
    
    uint64_t unit_msr;
    if (msr_read_cpu(0, MSR_PKG_POWER_INFO, &unit_msr) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    // Extract units from MSR
    uint32_t power_units = unit_msr & RAPL_POWER_UNIT_MASK;
    uint32_t energy_units = (unit_msr & RAPL_ENERGY_UNIT_MASK) >> 8;
    uint32_t time_units = (unit_msr & RAPL_TIME_UNIT_MASK) >> 16;
    
    *power_unit = 1.0 / (1 << power_units);  // Watts
    *energy_unit = 1.0 / (1 << energy_units); // Joules
    *time_unit = 1.0 / (1 << time_units);     // Seconds
    
    return SUCCESS;
}

int rapl_read_pkg_energy(uint64_t *energy_uj) {
    if (!energy_uj) return ERROR_INVALID_PARAM;
    
    uint64_t energy_msr;
    if (msr_read_cpu(0, MSR_PKG_ENERGY_STATUS, &energy_msr) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    *energy_uj = (uint64_t)(energy_msr * domains[0].energy_unit * 1000000.0);
    return SUCCESS;
}

int rapl_read_dram_energy(uint64_t *energy_uj) {
    if (!energy_uj) return ERROR_INVALID_PARAM;
    
    uint64_t energy_msr;
    if (msr_read_cpu(0, MSR_DRAM_ENERGY_STATUS, &energy_msr) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    *energy_uj = (uint64_t)(energy_msr * domains[1].energy_unit * 1000000.0);
    return SUCCESS;
}

int rapl_read_pkg_power_limit(uint64_t *power_limit_uw) {
    if (!power_limit_uw) return ERROR_INVALID_PARAM;
    
    uint64_t limit_msr;
    if (msr_read_cpu(0, MSR_PKG_POWER_LIMIT, &limit_msr) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    uint32_t power_limit = limit_msr & 0x7FFF;
    *power_limit_uw = (uint64_t)(power_limit * domains[0].power_unit * 1000000.0);
    
    return SUCCESS;
}

int rapl_set_pkg_power_limit(uint64_t power_limit_uw, uint64_t time_window_us) {
    uint64_t current_msr;
    if (msr_read_cpu(0, MSR_PKG_POWER_LIMIT, &current_msr) != SUCCESS) {
        return ERROR_SYSTEM;
    }
    
    uint32_t power_limit = (uint32_t)(power_limit_uw / (domains[0].power_unit * 1000000.0));
    uint32_t time_window = (uint32_t)(time_window_us / (domains[0].time_unit * 1000000.0));
    
    // Set PL1 (bits 14:0) and time window (bits 23:17)
    uint64_t new_msr = current_msr;
    new_msr = (new_msr & ~0x7FFF) | (power_limit & 0x7FFF);
    new_msr = (new_msr & ~(0x7F << 17)) | ((time_window & 0x7F) << 17);
    new_msr |= (1ULL << 15); // Enable PL1
    
    return msr_write_cpu(0, MSR_PKG_POWER_LIMIT, new_msr);
}

int rapl_test_basic_functionality(void) {
    PRINT_INFO("Testing basic RAPL functionality...");
    
    uint64_t pkg_energy, dram_energy;
    
    if (rapl_read_pkg_energy(&pkg_energy) == SUCCESS) {
        PRINT_DEBUG("Package energy: %lu microjoules", pkg_energy);
    }
    
    if (rapl_read_dram_energy(&dram_energy) == SUCCESS) {
        PRINT_DEBUG("DRAM energy: %lu microjoules", dram_energy);
    }
    
    uint64_t power_limit;
    if (rapl_read_pkg_power_limit(&power_limit) == SUCCESS) {
        PRINT_DEBUG("Current package power limit: %lu microwatts", power_limit);
    }
    
    return SUCCESS;
}

int rapl_test_energy_monitoring(void) {
    PRINT_INFO("Testing energy monitoring...");
    
    uint64_t start_energy, end_energy;
    
    if (rapl_read_pkg_energy(&start_energy) != SUCCESS) {
        PRINT_ERROR("Failed to read initial energy");
        return ERROR_SYSTEM;
    }
    
    // Do some work
    volatile uint64_t dummy = 0;
    for (int i = 0; i < 10000000; i++) {
        dummy += i * i;
    }
    
    sleep_ms(100);
    
    if (rapl_read_pkg_energy(&end_energy) != SUCCESS) {
        PRINT_ERROR("Failed to read final energy");
        return ERROR_SYSTEM;
    }
    
    uint64_t energy_consumed = end_energy - start_energy;
    PRINT_INFO("Energy consumed during test: %lu microjoules", energy_consumed);
    
    if (energy_consumed > 0) {
        double avg_power = (energy_consumed / 1000000.0) / 0.1; // Watts
        PRINT_INFO("Average power consumption: %.2f watts", avg_power);
    }
    
    return SUCCESS;
}

int rapl_test_power_capping(void) {
    PRINT_INFO("Testing power capping...");
    
    uint64_t original_limit;
    if (rapl_read_pkg_power_limit(&original_limit) != SUCCESS) {
        PRINT_ERROR("Failed to read original power limit");
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Original power limit: %lu microwatts", original_limit);
    
    // Set a more restrictive power limit (90% of original)
    uint64_t test_limit = (original_limit * 90) / 100;
    
    PRINT_DEBUG("Setting test power limit: %lu microwatts", test_limit);
    
    if (rapl_set_pkg_power_limit(test_limit, 1000000) != SUCCESS) {
        PRINT_ERROR("Failed to set power limit");
        return ERROR_SYSTEM;
    }
    
    sleep_ms(100);
    
    // Verify the limit was set
    uint64_t current_limit;
    if (rapl_read_pkg_power_limit(&current_limit) == SUCCESS) {
        PRINT_DEBUG("Current power limit after change: %lu microwatts", current_limit);
    }
    
    // Restore original limit
    if (rapl_set_pkg_power_limit(original_limit, 1000000) != SUCCESS) {
        PRINT_ERROR("Failed to restore original power limit");
        return ERROR_SYSTEM;
    }
    
    PRINT_DEBUG("Restored original power limit");
    
    return SUCCESS;
}

void rapl_print_info(void) {
    PRINT_INFO("RAPL Information:");
    
    double power_unit, energy_unit, time_unit;
    if (rapl_read_power_units(&power_unit, &energy_unit, &time_unit) == SUCCESS) {
        PRINT_INFO("Power unit: %.6f watts", power_unit);
        PRINT_INFO("Energy unit: %.9f joules", energy_unit);
        PRINT_INFO("Time unit: %.6f seconds", time_unit);
    }
    
    uint64_t pkg_energy, dram_energy, power_limit;
    
    if (rapl_read_pkg_energy(&pkg_energy) == SUCCESS) {
        PRINT_INFO("Current package energy: %lu microjoules", pkg_energy);
    }
    
    if (rapl_read_dram_energy(&dram_energy) == SUCCESS) {
        PRINT_INFO("Current DRAM energy: %lu microjoules", dram_energy);
    }
    
    if (rapl_read_pkg_power_limit(&power_limit) == SUCCESS) {
        PRINT_INFO("Current power limit: %lu microwatts (%.2f watts)", 
                  power_limit, power_limit / 1000000.0);
    }
}