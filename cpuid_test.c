
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <cpuid.h>

typedef struct {
    char vendor[13];
    char brand[49];
    int family;
    int model;
    int stepping;
} cpu_info_t;

void get_cpu_info(cpu_info_t *info) {
    uint32_t eax, ebx, ecx, edx;
    
    // Get vendor string
    __cpuid(0, eax, ebx, ecx, edx);
    memcpy(info->vendor, &ebx, 4);
    memcpy(info->vendor + 4, &edx, 4);
    memcpy(info->vendor + 8, &ecx, 4);
    info->vendor[12] = '\0';
    
    // Get family, model, stepping
    __cpuid(1, eax, ebx, ecx, edx);
    info->family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    info->model = ((eax >> 4) & 0xF) + ((eax >> 12) & 0xF0);
    info->stepping = eax & 0xF;
    
    // Get brand string
    if (__get_cpuid_max(0x80000000, NULL) >= 0x80000004) {
        uint32_t brand[12];
        __cpuid(0x80000002, brand[0], brand[1], brand[2], brand[3]);
        __cpuid(0x80000003, brand[4], brand[5], brand[6], brand[7]);
        __cpuid(0x80000004, brand[8], brand[9], brand[10], brand[11]);
        memcpy(info->brand, brand, 48);
        info->brand[48] = '\0';
    }
}

void check_basic_features() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("\n=== 基本 CPU 特性 ===\n");
    
    __cpuid(1, eax, ebx, ecx, edx);
    
    // ECX features
    printf("SSE3:        %s\n", (ecx & (1 << 0)) ? "支持" : "不支持");
    printf("PCLMUL:      %s\n", (ecx & (1 << 1)) ? "支持" : "不支持");
    printf("MONITOR:     %s\n", (ecx & (1 << 3)) ? "支持" : "不支持");
    printf("SSSE3:       %s\n", (ecx & (1 << 9)) ? "支持" : "不支持");
    printf("FMA:         %s\n", (ecx & (1 << 12)) ? "支持" : "不支持");
    printf("CMPXCHG16B:  %s\n", (ecx & (1 << 13)) ? "支持" : "不支持");
    printf("SSE4.1:      %s\n", (ecx & (1 << 19)) ? "支持" : "不支持");
    printf("SSE4.2:      %s\n", (ecx & (1 << 20)) ? "支持" : "不支持");
    printf("MOVBE:       %s\n", (ecx & (1 << 22)) ? "支持" : "不支持");
    printf("POPCNT:      %s\n", (ecx & (1 << 23)) ? "支持" : "不支持");
    printf("AES:         %s\n", (ecx & (1 << 25)) ? "支持" : "不支持");
    printf("XSAVE:       %s\n", (ecx & (1 << 26)) ? "支持" : "不支持");
    printf("OSXSAVE:     %s\n", (ecx & (1 << 27)) ? "支持" : "不支持");
    printf("AVX:         %s\n", (ecx & (1 << 28)) ? "支持" : "不支持");
    printf("F16C:        %s\n", (ecx & (1 << 29)) ? "支持" : "不支持");
    printf("RDRAND:      %s\n", (ecx & (1 << 30)) ? "支持" : "不支持");
    
    // EDX features
    printf("\nFPU:         %s\n", (edx & (1 << 0)) ? "支持" : "不支持");
    printf("VME:         %s\n", (edx & (1 << 1)) ? "支持" : "不支持");
    printf("PSE:         %s\n", (edx & (1 << 3)) ? "支持" : "不支持");
    printf("TSC:         %s\n", (edx & (1 << 4)) ? "支持" : "不支持");
    printf("MSR:         %s\n", (edx & (1 << 5)) ? "支持" : "不支持");
    printf("PAE:         %s\n", (edx & (1 << 6)) ? "支持" : "不支持");
    printf("CX8:         %s\n", (edx & (1 << 8)) ? "支持" : "不支持");
    printf("APIC:        %s\n", (edx & (1 << 9)) ? "支持" : "不支持");
    printf("SEP:         %s\n", (edx & (1 << 11)) ? "支持" : "不支持");
    printf("MTRR:        %s\n", (edx & (1 << 12)) ? "支持" : "不支持");
    printf("PGE:         %s\n", (edx & (1 << 13)) ? "支持" : "不支持");
    printf("MCA:         %s\n", (edx & (1 << 14)) ? "支持" : "不支持");
    printf("CMOV:        %s\n", (edx & (1 << 15)) ? "支持" : "不支持");
    printf("PAT:         %s\n", (edx & (1 << 16)) ? "支持" : "不支持");
    printf("PSE-36:      %s\n", (edx & (1 << 17)) ? "支持" : "不支持");
    printf("CLFLUSH:     %s\n", (edx & (1 << 19)) ? "支持" : "不支持");
    printf("MMX:         %s\n", (edx & (1 << 23)) ? "支持" : "不支持");
    printf("FXSR:        %s\n", (edx & (1 << 24)) ? "支持" : "不支持");
    printf("SSE:         %s\n", (edx & (1 << 25)) ? "支持" : "不支持");
    printf("SSE2:        %s\n", (edx & (1 << 26)) ? "支持" : "不支持");
    printf("HTT:         %s\n", (edx & (1 << 28)) ? "支持" : "不支持");
}

void check_extended_features() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("\n=== 扩展 CPU 特性 ===\n");
    
    if (__get_cpuid_max(7, NULL) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        
        // EBX features
        printf("FSGSBASE:    %s\n", (ebx & (1 << 0)) ? "支持" : "不支持");
        printf("TSC_ADJUST:  %s\n", (ebx & (1 << 1)) ? "支持" : "不支持");
        printf("SGX:         %s\n", (ebx & (1 << 2)) ? "支持" : "不支持");
        printf("BMI1:        %s\n", (ebx & (1 << 3)) ? "支持" : "不支持");
        printf("HLE:         %s\n", (ebx & (1 << 4)) ? "支持" : "不支持");
        printf("AVX2:        %s\n", (ebx & (1 << 5)) ? "支持" : "不支持");
        printf("SMEP:        %s\n", (ebx & (1 << 7)) ? "支持" : "不支持");
        printf("BMI2:        %s\n", (ebx & (1 << 8)) ? "支持" : "不支持");
        printf("ERMS:        %s\n", (ebx & (1 << 9)) ? "支持" : "不支持");
        printf("INVPCID:     %s\n", (ebx & (1 << 10)) ? "支持" : "不支持");
        printf("RTM:         %s\n", (ebx & (1 << 11)) ? "支持" : "不支持");
        printf("MPX:         %s\n", (ebx & (1 << 14)) ? "支持" : "不支持");
        printf("AVX512F:     %s\n", (ebx & (1 << 16)) ? "支持" : "不支持");
        printf("AVX512DQ:    %s\n", (ebx & (1 << 17)) ? "支持" : "不支持");
        printf("RDSEED:      %s\n", (ebx & (1 << 18)) ? "支持" : "不支持");
        printf("ADX:         %s\n", (ebx & (1 << 19)) ? "支持" : "不支持");
        printf("SMAP:        %s\n", (ebx & (1 << 20)) ? "支持" : "不支持");
        printf("AVX512_IFMA: %s\n", (ebx & (1 << 21)) ? "支持" : "不支持");
        printf("CLFLUSHOPT:  %s\n", (ebx & (1 << 23)) ? "支持" : "不支持");
        printf("CLWB:        %s\n", (ebx & (1 << 24)) ? "支持" : "不支持");
        printf("AVX512PF:    %s\n", (ebx & (1 << 26)) ? "支持" : "不支持");
        printf("AVX512ER:    %s\n", (ebx & (1 << 27)) ? "支持" : "不支持");
        printf("AVX512CD:    %s\n", (ebx & (1 << 28)) ? "支持" : "不支持");
        printf("SHA:         %s\n", (ebx & (1 << 29)) ? "支持" : "不支持");
        printf("AVX512BW:    %s\n", (ebx & (1 << 30)) ? "支持" : "不支持");
        printf("AVX512VL:    %s\n", (ebx & (1 << 31)) ? "支持" : "不支持");
        
        // ECX features
        printf("\nPREFETCHWT1: %s\n", (ecx & (1 << 0)) ? "支持" : "不支持");
        printf("AVX512_VBMI: %s\n", (ecx & (1 << 1)) ? "支持" : "不支持");
        printf("UMIP:        %s\n", (ecx & (1 << 2)) ? "支持" : "不支持");
        printf("PKU:         %s\n", (ecx & (1 << 3)) ? "支持" : "不支持");
        printf("OSPKE:       %s\n", (ecx & (1 << 4)) ? "支持" : "不支持");
        printf("WAITPKG:     %s\n", (ecx & (1 << 5)) ? "支持" : "不支持");
        printf("AVX512_VBMI2:%s\n", (ecx & (1 << 6)) ? "支持" : "不支持");
        printf("CET_SS:      %s\n", (ecx & (1 << 7)) ? "支持" : "不支持");
        printf("GFNI:        %s\n", (ecx & (1 << 8)) ? "支持" : "不支持");
        printf("VAES:        %s\n", (ecx & (1 << 9)) ? "支持" : "不支持");
        printf("VPCLMULQDQ:  %s\n", (ecx & (1 << 10)) ? "支持" : "不支持");
        printf("AVX512_VNNI: %s\n", (ecx & (1 << 11)) ? "支持" : "不支持");
        printf("AVX512_BITALG:%s\n", (ecx & (1 << 12)) ? "支持" : "不支持");
        printf("AVX512_VPOPCNTDQ:%s\n", (ecx & (1 << 14)) ? "支持" : "不支持");
        printf("RDPID:       %s\n", (ecx & (1 << 22)) ? "支持" : "不支持");
        printf("CLDEMOTE:    %s\n", (ecx & (1 << 25)) ? "支持" : "不支持");
        printf("MOVDIRI:     %s\n", (ecx & (1 << 27)) ? "支持" : "不支持");
        printf("MOVDIR64B:   %s\n", (ecx & (1 << 28)) ? "支持" : "不支持");
        printf("SGX_LC:      %s\n", (ecx & (1 << 30)) ? "支持" : "不支持");
        
        // EDX features
        printf("\nAVX512_4VNNIW:%s\n", (edx & (1 << 2)) ? "支持" : "不支持");
        printf("AVX512_4FMAPS:%s\n", (edx & (1 << 3)) ? "支持" : "不支持");
        printf("FSRM:        %s\n", (edx & (1 << 4)) ? "支持" : "不支持");
        printf("AVX512_VP2INTERSECT:%s\n", (edx & (1 << 8)) ? "支持" : "不支持");
        printf("MD_CLEAR:    %s\n", (edx & (1 << 10)) ? "支持" : "不支持");
        printf("SERIALIZE:   %s\n", (edx & (1 << 14)) ? "支持" : "不支持");
        printf("HYBRID:      %s\n", (edx & (1 << 15)) ? "支持" : "不支持");
        printf("TSXLDTRK:    %s\n", (edx & (1 << 16)) ? "支持" : "不支持");
        printf("PCONFIG:     %s\n", (edx & (1 << 18)) ? "支持" : "不支持");
        printf("CET_IBT:     %s\n", (edx & (1 << 20)) ? "支持" : "不支持");
        printf("AMX_BF16:    %s\n", (edx & (1 << 22)) ? "支持" : "不支持");
        printf("AVX512_FP16: %s\n", (edx & (1 << 23)) ? "支持" : "不支持");
        printf("AMX_TILE:    %s\n", (edx & (1 << 24)) ? "支持" : "不支持");
        printf("AMX_INT8:    %s\n", (edx & (1 << 25)) ? "支持" : "不支持");
    }
}

void check_power_features() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("\n=== 电源管理特性 ===\n");
    
    if (__get_cpuid_max(0, NULL) >= 6) {
        __cpuid(6, eax, ebx, ecx, edx);
        
        printf("数字温度传感器:     %s\n", (eax & (1 << 0)) ? "支持" : "不支持");
        printf("Turbo Boost:       %s\n", (eax & (1 << 1)) ? "支持" : "不支持");
        printf("ARAT:              %s\n", (eax & (1 << 2)) ? "支持" : "不支持");
        printf("PLN:               %s\n", (eax & (1 << 4)) ? "支持" : "不支持");
        printf("ECMD:              %s\n", (eax & (1 << 5)) ? "支持" : "不支持");
        printf("PTM:               %s\n", (eax & (1 << 6)) ? "支持" : "不支持");
        printf("HWP:               %s\n", (eax & (1 << 7)) ? "支持" : "不支持");
        printf("HWP 通知:          %s\n", (eax & (1 << 8)) ? "支持" : "不支持");
        printf("HWP 活动窗口:       %s\n", (eax & (1 << 9)) ? "支持" : "不支持");
        printf("HWP 能量性能偏好:   %s\n", (eax & (1 << 10)) ? "支持" : "不支持");
        printf("HWP 包级别请求:     %s\n", (eax & (1 << 11)) ? "支持" : "不支持");
        printf("HDC:               %s\n", (eax & (1 << 13)) ? "支持" : "不支持");
        printf("Turbo Boost 3.0:   %s\n", (eax & (1 << 14)) ? "支持" : "不支持");
        printf("HWP 能力:          %s\n", (eax & (1 << 15)) ? "支持" : "不支持");
        printf("HWP PECI:          %s\n", (eax & (1 << 16)) ? "支持" : "不支持");
        printf("柔性 HWP:          %s\n", (eax & (1 << 17)) ? "支持" : "不支持");
        printf("快速 IA32_HWP_REQUEST: %s\n", (eax & (1 << 18)) ? "支持" : "不支持");
        printf("忽略空闲 HWP 请求:  %s\n", (eax & (1 << 20)) ? "支持" : "不支持");
        
        printf("\n中断阈值数量: %d\n", ebx & 0xF);
        
        printf("\n硬件协调反馈:      %s\n", (ecx & (1 << 0)) ? "支持" : "不支持");
        printf("性能-能量偏好:     %s\n", (ecx & (1 << 3)) ? "支持" : "不支持");
    }
}

void check_cache_info() {
    uint32_t eax, ebx, ecx, edx;
    int i = 0;
    
    printf("\n=== 缓存信息 ===\n");
    
    while (1) {
        __cpuid_count(4, i, eax, ebx, ecx, edx);
        
        int cache_type = eax & 0x1F;
        if (cache_type == 0) break;
        
        int cache_level = (eax >> 5) & 0x7;
        int ways = ((ebx >> 22) & 0x3FF) + 1;
        int partitions = ((ebx >> 12) & 0x3FF) + 1;
        int line_size = (ebx & 0xFFF) + 1;
        int sets = ecx + 1;
        int size = ways * partitions * line_size * sets;
        
        const char *type_str;
        switch (cache_type) {
            case 1: type_str = "数据"; break;
            case 2: type_str = "指令"; break;
            case 3: type_str = "统一"; break;
            default: type_str = "未知"; break;
        }
        
        printf("L%d %s缓存:\n", cache_level, type_str);
        printf("  大小: %d KB\n", size / 1024);
        printf("  路数: %d\n", ways);
        printf("  行大小: %d 字节\n", line_size);
        printf("  组数: %d\n", sets);
        
        i++;
    }
}

void check_virtualization() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("\n=== 虚拟化特性 ===\n");
    
    __cpuid(1, eax, ebx, ecx, edx);
    printf("VMX (Intel VT-x):  %s\n", (ecx & (1 << 5)) ? "支持" : "不支持");
    
    if (__get_cpuid_max(0x80000000, NULL) >= 0x80000001) {
        __cpuid(0x80000001, eax, ebx, ecx, edx);
        printf("SVM (AMD-V):       %s\n", (ecx & (1 << 2)) ? "支持" : "不支持");
    }
}

void check_security_features() {
    uint32_t eax, ebx, ecx, edx;
    
    printf("\n=== 安全特性 ===\n");
    
    // From CPUID leaf 7
    if (__get_cpuid_max(7, NULL) >= 7) {
        __cpuid_count(7, 0, eax, ebx, ecx, edx);
        
        printf("SMEP:              %s\n", (ebx & (1 << 7)) ? "支持" : "不支持");
        printf("SMAP:              %s\n", (ebx & (1 << 20)) ? "支持" : "不支持");
        printf("SGX:               %s\n", (ebx & (1 << 2)) ? "支持" : "不支持");
        printf("CET Shadow Stack:  %s\n", (ecx & (1 << 7)) ? "支持" : "不支持");
        printf("CET IBT:           %s\n", (edx & (1 << 20)) ? "支持" : "不支持");
    }
    
    // From extended CPUID
    if (__get_cpuid_max(0x80000000, NULL) >= 0x80000001) {
        __cpuid(0x80000001, eax, ebx, ecx, edx);
        printf("NX/XD:             %s\n", (edx & (1 << 20)) ? "支持" : "不支持");
    }
}

int main() {
    cpu_info_t info;
    
    printf("=== CPU 特性检测工具 ===\n\n");
    
    get_cpu_info(&info);
    
    printf("CPU 厂商: %s\n", info.vendor);
    printf("CPU 型号: %s\n", info.brand);
    printf("家族: %d, 型号: %d, 步进: %d\n", info.family, info.model, info.stepping);
    
    check_basic_features();
    check_extended_features();
    check_power_features();
    check_cache_info();
    check_virtualization();
    check_security_features();
    
    return 0;
}