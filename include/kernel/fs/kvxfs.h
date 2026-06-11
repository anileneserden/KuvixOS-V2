#ifndef KVXFS_H
#define KVXFS_H

#include <stdint.h>

int kvxfs_init(void);
int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size);
int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size);
int kvxfs_exists(const char* path);
int kvxfs_is_dir(const char* path);
int kvxfs_force_format(void);
int kvxfs_mkdir(const char* path);
int kvxfs_remove(const char* path);
void kvxfs_list_all(const char* filter_path);
int kvxfs_tree(const char* root_path);
int kvxfs_format(void);
int kvxfs_open(const char* path);
void kvxfs_close(int fd);
int kvxfs_read(int fd, void* out, uint32_t n, uint32_t* out_nread);
uint32_t kvxfs_get_size(const char* path);

#endif