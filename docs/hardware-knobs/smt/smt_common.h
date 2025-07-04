#ifndef SMT_COMMON_H
#define SMT_COMMON_H

#include "../common/common.h"
#include "../common/msr_utils.h"

// SMT control paths
#define SMT_CONTROL_PATH "/sys/devices/system/cpu/smt/control"
#define SMT_ACTIVE_PATH "/sys/devices/system/cpu/smt/active"
#define CPU_ONLINE_PATH "/sys/devices/system/cpu/cpu%d/online"

typedef enum {
    SMT_ON,
    SMT_OFF,
    SMT_FORCEOFF,
    SMT_NOTSUPPORTED
} smt_state_t;

// Shared function declarations
int smt_check_support(void);
smt_state_t smt_get_state(void);
int smt_set_state(smt_state_t state);
int smt_get_active_threads(void);

#endif /* SMT_COMMON_H */