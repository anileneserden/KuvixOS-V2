#pragma once
#include <stdint.h>

#ifndef RAMFS_MAX_FILES
#define RAMFS_MAX_FILES  64
#endif

#ifndef RAMFS_PATH_MAX
#define RAMFS_PATH_MAX   96
#endif

#ifndef RAMFS_POOL_SIZE
#define RAMFS_POOL_SIZE  (64 * 1024) // 64 KB
#endif

// --- DÜĞÜM TİPLERİ ---
#define RAMFS_T_FILE 0
#define RAMFS_T_DIR  1

typedef struct {
    char     path[RAMFS_PATH_MAX];
    uint32_t off;
    uint32_t cap;
    uint32_t size;
    uint8_t  used;
    uint8_t  type;  // ⭐ Klasör mü dosya mı olduğunu anlamak için eklendi
} ramfs_node_t;

void     ramfs_init(void);

int      ramfs_exists(const char* path);
int      ramfs_list(const char* dir_prefix, int (*cb)(const char* path, uint32_t size, void* u), void* u);

// --- VFS'NİN İSTEDİĞİ FONKSİYONLAR ---
int      ramfs_is_dir(const char* path);
int      ramfs_mkdir(const char* path);
int      ramfs_remove(const char* path);

// stream I/O
int      ramfs_open(const char* path, int create, int* out_fd);
int      ramfs_read(int fd, void* out, uint32_t n, uint32_t* out_nread);
int      ramfs_write(int fd, const void* in, uint32_t n, uint32_t* out_nwritten);
void     ramfs_seek(int fd, uint32_t pos);
uint32_t ramfs_tell(int fd);
uint32_t ramfs_size(int fd);
void     ramfs_close(int fd);

// helpers
int      ramfs_write_all(const char* path, const void* data, uint32_t size);
int      ramfs_read_all(const char* path, void* out, uint32_t cap, uint32_t* out_size);