#include "smt_common.h"

int smt_check_support(void) {
    // Check if SMT control interface exists
    if (check_file_exists(SMT_CONTROL_PATH) != SUCCESS) {
        PRINT_ERROR("SMT control interface not found at %s", SMT_CONTROL_PATH);
        return ERROR_NOT_SUPPORTED;
    }
    
    // Check if CPU supports SMT
    if (check_cpu_feature("ht") != SUCCESS) {
        PRINT_ERROR("CPU does not support Hyper-Threading");
        return ERROR_NOT_SUPPORTED;
    }
    
    return SUCCESS;
}

smt_state_t smt_get_state(void) {
    char state_str[32];
    if (read_file_str(SMT_CONTROL_PATH, state_str, sizeof(state_str)) != SUCCESS) {
        return SMT_NOTSUPPORTED;
    }
    
    if (strcmp(state_str, "on") == 0) {
        return SMT_ON;
    } else if (strcmp(state_str, "off") == 0) {
        return SMT_OFF;
    } else if (strcmp(state_str, "forceoff") == 0) {
        return SMT_FORCEOFF;
    } else if (strcmp(state_str, "notsupported") == 0) {
        return SMT_NOTSUPPORTED;
    }
    
    return SMT_NOTSUPPORTED;
}

int smt_set_state(smt_state_t state) {
    const char *state_str;
    
    switch (state) {
        case SMT_ON:
            state_str = "on";
            break;
        case SMT_OFF:
            state_str = "off";
            break;
        case SMT_FORCEOFF:
            state_str = "forceoff";
            break;
        default:
            return ERROR_INVALID_PARAM;
    }
    
    return write_file_str(SMT_CONTROL_PATH, state_str);
}

int smt_get_active_threads(void) {
    int active_threads;
    if (read_file_int(SMT_ACTIVE_PATH, &active_threads) != SUCCESS) {
        return -1;
    }
    return active_threads;
}