// src/lib/fs/ramfs.c
#include <kernel/fs/ramfs.h>

static ramfs_node_t g_nodes[RAMFS_MAX_FILES];

static uint8_t  g_pool[RAMFS_POOL_SIZE];
static uint32_t g_pool_top = 0;

typedef struct {
    int      used;
    int      node_idx;
    uint32_t pos;
} ramfs_fd_t;

#ifndef RAMFS_MAX_FDS
#define RAMFS_MAX_FDS 32
#endif

static ramfs_fd_t g_fds[RAMFS_MAX_FDS];

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

static uint32_t strlcpy0(char* dst, const char* src, uint32_t cap) {
    if (!cap) return 0;
    uint32_t i = 0;
    while (src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
    return i;
}

static int starts_with(const char* s, const char* pref) {
    while (*pref) {
        if (*s++ != *pref++) return 0;
    }
    return 1;
}

static int find_node(const char* path) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (g_nodes[i].used && streq(g_nodes[i].path, path)) return i;
    }
    return -1;
}

static int alloc_node(const char* path) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!g_nodes[i].used) {
            g_nodes[i].used = 1;
            g_nodes[i].off  = 0;
            g_nodes[i].cap  = 0;
            g_nodes[i].size = 0;
            strlcpy0(g_nodes[i].path, path, RAMFS_PATH_MAX);
            return i;
        }
    }
    return -1;
}

static int alloc_fd(int node_idx) {
    for (int i = 0; i < RAMFS_MAX_FDS; i++) {
        if (!g_fds[i].used) {
            g_fds[i].used = 1;
            g_fds[i].node_idx = node_idx;
            g_fds[i].pos = 0;
            return i;
        }
    }
    return -1;
}

static void free_fd(int fd) {
    if (fd < 0 || fd >= RAMFS_MAX_FDS) return;
    g_fds[fd].used = 0;
    g_fds[fd].node_idx = -1;
    g_fds[fd].pos = 0;
}

void ramfs_init(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) g_nodes[i].used = 0;
    for (int i = 0; i < RAMFS_MAX_FDS; i++) g_fds[i].used = 0;
    g_pool_top = 0;
}

int ramfs_exists(const char* path) {
    return find_node(path) >= 0;
}

int ramfs_list(const char* dir_prefix, int (*cb)(const char* path, uint32_t size, void* u), void* u) {
    if (!cb) return 0;
    const char* pref = dir_prefix ? dir_prefix : "";
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!g_nodes[i].used) continue;
        if (pref[0] && !starts_with(g_nodes[i].path, pref)) continue;
        if (!cb(g_nodes[i].path, g_nodes[i].size, u)) return 0; // cb false -> stop
    }
    return 1;
}

int ramfs_open(const char* path, int create, int* out_fd) {
    if (!path || !out_fd) return 0;

    int idx = find_node(path);
    if (idx < 0) {
        if (!create) return 0;
        idx = alloc_node(path);
        if (idx < 0) return 0;
    }

    int fd = alloc_fd(idx);
    if (fd < 0) return 0;

    *out_fd = fd;
    return 1;
}

uint32_t ramfs_tell(int fd) {
    if (fd < 0 || fd >= RAMFS_MAX_FDS || !g_fds[fd].used) return 0;
    return g_fds[fd].pos;
}

void ramfs_seek(int fd, uint32_t pos) {
    if (fd < 0 || fd >= RAMFS_MAX_FDS || !g_fds[fd].used) return;
    g_fds[fd].pos = pos;
}

uint32_t ramfs_size(int fd) {
    if (fd < 0 || fd >= RAMFS_MAX_FDS || !g_fds[fd].used) return 0;
    int idx = g_fds[fd].node_idx;
    if (idx < 0) return 0;
    return g_nodes[idx].size;
}

void ramfs_close(int fd) {
    free_fd(fd);
}

int ramfs_read(int fd, void* out, uint32_t n, uint32_t* out_nread) {
    if (out_nread) *out_nread = 0;
    if (fd < 0 || fd >= RAMFS_MAX_FDS || !g_fds[fd].used) return 0;
    if (!out || n == 0) return 1;

    int idx = g_fds[fd].node_idx;
    ramfs_node_t* node = &g_nodes[idx];

    uint32_t pos = g_fds[fd].pos;
    if (pos >= node->size) return 1;

    uint32_t can = node->size - pos;
    if (n > can) n = can;

    // memcpy
    uint8_t* d = (uint8_t*)out;
    const uint8_t* s = &g_pool[node->off + pos];
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];

    g_fds[fd].pos += n;
    if (out_nread) *out_nread = n;
    return 1;
}

static int ensure_capacity(ramfs_node_t* node, uint32_t need_cap) {
    if (need_cap <= node->cap) return 1;

    // simple "bump allocate": allocate new block, copy old data, leak old block (ok for now)
    uint32_t newcap = node->cap ? node->cap : 64;
    while (newcap < need_cap) newcap *= 2;
    if (newcap < need_cap) newcap = need_cap;

    if (g_pool_top + newcap > RAMFS_POOL_SIZE) return 0;

    uint32_t newoff = g_pool_top;
    g_pool_top += newcap;

    // copy old data
    for (uint32_t i = 0; i < node->size; i++) {
        g_pool[newoff + i] = g_pool[node->off + i];
    }

    node->off = newoff;
    node->cap = newcap;
    return 1;
}

int ramfs_write(int fd, const void* in, uint32_t n, uint32_t* out_nwritten) {
    if (out_nwritten) *out_nwritten = 0;
    if (fd < 0 || fd >= RAMFS_MAX_FDS || !g_fds[fd].used) return 0;
    if (!in || n == 0) return 1;

    int idx = g_fds[fd].node_idx;
    ramfs_node_t* node = &g_nodes[idx];

    uint32_t pos = g_fds[fd].pos;
    uint32_t end = pos + n;

    if (!ensure_capacity(node, end)) return 0;

    const uint8_t* s = (const uint8_t*)in;
    uint8_t* d = &g_pool[node->off + pos];
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];

    g_fds[fd].pos += n;
    if (end > node->size) node->size = end;

    if (out_nwritten) *out_nwritten = n;
    return 1;
}

int ramfs_write_all(const char* path, const void* data, uint32_t size) {
    int fd;
    if (!ramfs_open(path, 1, &fd)) return 0;
    ramfs_seek(fd, 0);

    // truncate by resetting size (keep capacity)
    int idx = g_fds[fd].node_idx;
    g_nodes[idx].size = 0;

    uint32_t nw = 0;
    int ok = ramfs_write(fd, data, size, &nw);
    ramfs_close(fd);
    return ok && (nw == size);
}

int ramfs_read_all(const char* path, void* out, uint32_t cap, uint32_t* out_size) {
    if (out_size) *out_size = 0;
    int idx = find_node(path);
    if (idx < 0) return 0;

    ramfs_node_t* node = &g_nodes[idx];
    uint32_t n = node->size;
    if (n > cap) n = cap;

    uint8_t* d = (uint8_t*)out;
    const uint8_t* s = &g_pool[node->off];
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];

    if (out_size) *out_size = n;
    return 1;
}

int ramfs_is_dir(const char* path) {
    // Şimdilik her şeyin dosya olmadığını varsayalım
    // İleride gerçek node kontrolü ekleyeceğiz
    return 0; 
}

int ramfs_mkdir(const char* path) {
    // Şimdilik klasör oluşturmayı kapatıyoruz
    return -1; 
}