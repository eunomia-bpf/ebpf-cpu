#ifndef PREFETCH_COMMON_H
#define PREFETCH_COMMON_H

#include "../common/common.h"
#include "../common/msr_utils.h"

// Intel prefetch control MSR definitions
#define MSR_MISC_FEATURES_ENABLES   0x140

// Prefetch control bit definitions
#define PREFETCH_L2_STREAM_HW_DISABLE   (1ULL << 0)
#define PREFETCH_L2_STREAM_ADJ_DISABLE  (1ULL << 1)
#define PREFETCH_DCU_STREAM_DISABLE     (1ULL << 2)
#define PREFETCH_DCU_IP_DISABLE         (1ULL << 3)

// Shared function declarations
int prefetch_check_support(void);
int prefetch_read_config(uint64_t *config);
int prefetch_write_config(uint64_t config);

#endif /* PREFETCH_COMMON_H */