#ifndef KVXFS_H
#define KVXFS_H

#include <stdint.h>

// VFS ve KVXFS arasındaki köprü
int kvxfs_init(void);
int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size);
int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size);
int kvxfs_exists(const char* path);
int kvxfs_force_format(void);
int kvxfs_mkdir(const char* path);
void kvxfs_list_all(const char* filter_path);

#endif