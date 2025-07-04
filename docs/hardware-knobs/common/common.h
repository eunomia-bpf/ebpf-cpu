#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

// Add GNU source for additional features
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// Common error codes
#define SUCCESS 0
#define ERROR_PERMISSION -1
#define ERROR_NOT_SUPPORTED -2
#define ERROR_INVALID_PARAM -3
#define ERROR_SYSTEM -4

// Common print macros
#define PRINT_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define PRINT_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define PRINT_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define PRINT_SUCCESS(fmt, ...) printf("[SUCCESS] " fmt "\n", ##__VA_ARGS__)

// Common utility functions
int check_root_permission(void);
int check_file_exists(const char *path);
int read_file_int(const char *path, int *value);
int write_file_int(const char *path, int value);
int read_file_str(const char *path, char *buffer, size_t size);
int write_file_str(const char *path, const char *str);

// CPU information
int get_cpu_count(void);
int get_cpu_vendor(char *vendor, size_t size);
int check_cpu_feature(const char *feature);

// Time utilities
uint64_t get_timestamp_us(void);
void sleep_ms(int ms);

#endif /* COMMON_H */