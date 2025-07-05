#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define MSR_IA32_L3_MASK_0 0xC90

int main() {
    int fd;
    uint64_t value;
    
    printf("Testing MSR access for RDT...\n");
    
    // Open MSR for CPU 0
    fd = open("/dev/cpu/0/msr", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    
    // Try to read first
    if (pread(fd, &value, sizeof(value), MSR_IA32_L3_MASK_0) == sizeof(value)) {
        printf("Read MSR 0x%x: 0x%lx\n", MSR_IA32_L3_MASK_0, value);
        
        // Try to write the same value back
        if (pwrite(fd, &value, sizeof(value), MSR_IA32_L3_MASK_0) == sizeof(value)) {
            printf("Write MSR 0x%x successful\n", MSR_IA32_L3_MASK_0);
        } else {
            printf("Write MSR 0x%x failed: %s\n", MSR_IA32_L3_MASK_0, strerror(errno));
            
            // Try with a different approach - seek then write
            if (lseek(fd, MSR_IA32_L3_MASK_0, SEEK_SET) >= 0) {
                if (write(fd, &value, sizeof(value)) == sizeof(value)) {
                    printf("Write with seek successful\n");
                } else {
                    printf("Write with seek failed: %s\n", strerror(errno));
                }
            }
        }
    } else {
        printf("Read MSR 0x%x failed: %s\n", MSR_IA32_L3_MASK_0, strerror(errno));
    }
    
    close(fd);
    return 0;
}