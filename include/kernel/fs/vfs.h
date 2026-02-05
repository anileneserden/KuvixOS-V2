#pragma once
#include <stdint.h>

// --- Eksik olan tanımlar eklendi ---
#define VFS_PATH_MAX 256

#define VFS_T_FILE   1
#define VFS_T_DIR    2

typedef struct {
    int      type;     // VFS_T_FILE veya VFS_T_DIR
    uint32_t size;
    int      backend;  // 1: RAM, 2: TOY, 3: KVX
} vfs_stat_t;

// --- Mevcut yapın ---
typedef struct vfs_file vfs_file_t;

// Open flags
#define VFS_O_RDONLY  0
#define VFS_O_WRONLY  1
#define VFS_O_RDWR    2
#define VFS_O_CREAT   0x100

void vfs_init(void);

// Stream I/O
int  vfs_open(const char* path, int flags, vfs_file_t** out);
int  vfs_read(vfs_file_t* f, void* out, uint32_t n, uint32_t* out_nread);
int  vfs_write(vfs_file_t* f, const void* in, uint32_t n, uint32_t* out_nwritten);
void vfs_close(vfs_file_t* f);

// Helpers
int  vfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size);
int  vfs_write_all(const char* path, const uint8_t* data, uint32_t size);
int  vfs_resolve_path(const char* in, char* out, uint32_t cap);
int  vfs_stat(const char* path, vfs_stat_t* st);
int  vfs_mkdir(const char* path);
int  vfs_remove(const char* path);
int vfs_rename(const char* old_path, const char* new_path);

// CWD (Current Working Directory)
const char* vfs_get_cwd(void);
int         vfs_set_cwd(const char* path);
void        vfs_cd_parent(void);

// List
int  vfs_list(const char* dir_prefix, int (*cb)(const char* path, uint32_t size, void* u), void* u);