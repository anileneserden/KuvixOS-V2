#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/fs/toyfs.h>
#include <kernel/block/block.h>
#include <arch/x86/io.h>
#include <lib/string.h>
#include <stdint.h>

// ==========================================================
// Helpers
// ==========================================================
static void path_pop(char *path);
static void copy_str(char* dst, const char* src, uint32_t cap);

static void mem_zero_local(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

static int str_starts_exact(const char* path, const char* prefix, uint32_t prefix_len) {
    if (!path) return 0;
    if (strncmp(path, prefix, prefix_len) != 0) return 0;
    return (path[prefix_len] == 0 || path[prefix_len] == '/');
}

// ==========================================================
// Removable mount view: /removable -> ToyFS root
// ==========================================================
#define REMOUNT_PREFIX "/removable"
#define REMOUNT_PREFIX_LEN 10

static int is_removable_path(const char* path) {
    return str_starts_exact(path, REMOUNT_PREFIX, REMOUNT_PREFIX_LEN);
}

// "/removable" -> "/"
// "/removable/abc" -> "/abc"
static void removable_to_toy(const char* in, char* out, uint32_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;

    if (!in || !is_removable_path(in)) {
        copy_str(out, in, cap);
        return;
    }

    const char* p = in + REMOUNT_PREFIX_LEN;
    if (*p == 0) {
        copy_str(out, "/", cap);
        return;
    }

    copy_str(out, p, cap);
}

typedef struct {
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} rem_wrap_t;

// ToyFS path -> /removable + toy_path
static int rem_cb_prefix(const char* toy_path, uint32_t size, void* u2) {
    rem_wrap_t* w = (rem_wrap_t*)u2;
    if (!w || !w->cb) return 0;

    char outp[VFS_PATH_MAX];
    outp[0] = 0;

    copy_str(outp, REMOUNT_PREFIX, sizeof(outp));

    if (toy_path && strcmp(toy_path, "/") != 0) {
        strncat(outp, toy_path, sizeof(outp) - strlen(outp) - 1);
    }

    return w->cb(outp, size, w->u);
}

// ==========================================================
// KVXFS on-disk metadata mirror (for list/stat/remove/rename)
// NOTE: Same layout as kvxfs.c
// ==========================================================
#define KVX_MAGIC "KVXFS1"
#define KVX_MAX_FILES 32
#define KVX_META_LBA 2048
#define KVX_META_SECTORS 8
#define KVX_DATA_LBA 2100

typedef struct {
    char     path[64];
    uint32_t start_lba;
    uint32_t size;
    uint8_t  used;
    uint8_t  _pad[3];
} __attribute__((packed)) kvx_ent_disk_t;

typedef struct {
    char     magic[8];
    uint32_t file_count;
    uint32_t next_free_lba;
    kvx_ent_disk_t ent[KVX_MAX_FILES];
} __attribute__((packed)) kvx_meta_disk_t;

static uint8_t g_kvx_meta_buf[KVX_META_SECTORS * 512];

static int kvx_meta_read_raw(kvx_meta_disk_t* out) {
    if (!out) return 0;
    mem_zero_local(g_kvx_meta_buf, sizeof(g_kvx_meta_buf));
    if (!block_read(KVX_META_LBA, KVX_META_SECTORS, g_kvx_meta_buf)) return 0;
    memcpy(out, g_kvx_meta_buf, sizeof(kvx_meta_disk_t));
    if (strncmp(out->magic, KVX_MAGIC, 6) != 0) return 0;
    return 1;
}

static int kvx_meta_write_raw(const kvx_meta_disk_t* in) {
    if (!in) return 0;
    mem_zero_local(g_kvx_meta_buf, sizeof(g_kvx_meta_buf));
    memcpy(g_kvx_meta_buf, in, sizeof(kvx_meta_disk_t));

    for (volatile int i = 0; i < 30000; i++) io_wait();
    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_kvx_meta_buf)) return 0;
    for (volatile int i = 0; i < 50000; i++) io_wait();

    return 1;
}

static int kvx_find_real(const char* real_path, kvx_meta_disk_t* meta_out, int* idx_out) {
    kvx_meta_disk_t meta;
    if (!kvx_meta_read_raw(&meta)) return 0;

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (meta.ent[i].used && strcmp(meta.ent[i].path, real_path) == 0) {
            if (meta_out) *meta_out = meta;
            if (idx_out) *idx_out = i;
            return 1;
        }
    }
    return 0;
}

static int kvx_is_dir_real(const char* real_path) {
    kvx_meta_disk_t meta;
    int idx = -1;
    if (!kvx_find_real(real_path, &meta, &idx)) return 0;
    return (meta.ent[idx].size == 0xFFFFFFFFU);
}

static int kvx_exists_real(const char* real_path) {
    kvx_meta_disk_t meta;
    int idx = -1;
    return kvx_find_real(real_path, &meta, &idx);
}

static int kvx_file_size_real(const char* real_path, uint32_t* out_size) {
    kvx_meta_disk_t meta;
    int idx = -1;
    if (!kvx_find_real(real_path, &meta, &idx)) return 0;
    if (meta.ent[idx].size == 0xFFFFFFFFU) return 0;
    if (out_size) *out_size = meta.ent[idx].size;
    return 1;
}

static int path_is_direct_child_of(const char* parent, const char* child) {
    if (!parent || !child) return 0;

    uint32_t plen = strlen(parent);
    if (strcmp(parent, "/") == 0) {
        if (child[0] != '/' || child[1] == 0) return 0;
        for (uint32_t i = 1; child[i]; i++) {
            if (child[i] == '/') return 0;
        }
        return 1;
    }

    if (strncmp(child, parent, plen) != 0) return 0;
    if (child[plen] != '/') return 0;

    const char* rest = child + plen + 1;
    if (*rest == 0) return 0;

    for (int i = 0; rest[i]; i++) {
        if (rest[i] == '/') return 0;
    }
    return 1;
}

typedef struct {
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} kvx_iter_wrap_t;

static int kvx_iter_real(const char* real_dir,
                         int (*cb)(const char* real_path, uint32_t size, void* u),
                         void* u) {
    if (!real_dir) return 0;

    kvx_meta_disk_t meta;
    if (!kvx_meta_read_raw(&meta)) return 0;

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!meta.ent[i].used) continue;
        if (!path_is_direct_child_of(real_dir, meta.ent[i].path)) continue;

        if (cb) {
            if (!cb(meta.ent[i].path, meta.ent[i].size, u)) return 0;
        }
    }
    return 1;
}

static int kvx_remove_real(const char* real_path) {
    kvx_meta_disk_t meta;
    if (!kvx_meta_read_raw(&meta)) return 0;

    int found = -1;
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (meta.ent[i].used && strcmp(meta.ent[i].path, real_path) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) return 0;

    if (meta.ent[found].size == 0xFFFFFFFFU) {
        // dizinse çocuk var mı?
        for (int i = 0; i < KVX_MAX_FILES; i++) {
            if (!meta.ent[i].used) continue;
            if (path_is_direct_child_of(real_path, meta.ent[i].path)) {
                return 0; // non-empty dir silme yok
            }
        }
    }

    mem_zero_local(&meta.ent[found], sizeof(kvx_ent_disk_t));
    if (meta.file_count > 0) meta.file_count--;
    return kvx_meta_write_raw(&meta);
}

static int kvx_rename_real(const char* old_real, const char* new_real) {
    if (!old_real || !new_real) return 0;

    kvx_meta_disk_t meta;
    if (!kvx_meta_read_raw(&meta)) return 0;

    int src = -1;
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (meta.ent[i].used && strcmp(meta.ent[i].path, old_real) == 0) {
            src = i;
            break;
        }
    }
    if (src < 0) return 0;

    // hedef zaten varsa iptal
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (meta.ent[i].used && strcmp(meta.ent[i].path, new_real) == 0) {
            return 0;
        }
    }

    // basit dosya/tek dizin rename
    mem_zero_local(meta.ent[src].path, sizeof(meta.ent[src].path));
    strncpy(meta.ent[src].path, new_real, sizeof(meta.ent[src].path) - 1);

    return kvx_meta_write_raw(&meta);
}

// ==========================================================
// Path classes
// ==========================================================
static int is_tmp_path(const char* path) {
    return str_starts_exact(path, "/tmp", 4);
}

static int is_home_path(const char* path) {
    return str_starts_exact(path, "/home", 5);
}

static int is_apps_path(const char* path) {
    return str_starts_exact(path, "/apps", 5);
}

static int is_persist_path_vfs(const char* path) {
    return str_starts_exact(path, "/persist", 8);
}

static int is_kvx_path(const char* path) {
    return is_home_path(path) || is_apps_path(path) || is_persist_path_vfs(path);
}

// logical -> real kvx path
// /home/x   -> /persist/home/x
// /apps/x   -> /persist/apps/x
// /persist/x -> /persist/x
static void vfs_to_kvx_path(const char* in, char* out, uint32_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;

    if (!in) return;

    if (is_home_path(in) || is_apps_path(in)) {
        copy_str(out, "/persist", cap);
        strncat(out, in, cap - strlen(out) - 1);
        return;
    }

    if (is_persist_path_vfs(in)) {
        copy_str(out, in, cap);
        return;
    }

    copy_str(out, in, cap);
}

// real kvx path -> logical vfs path
// when listing /home or /apps we strip "/persist"
// when listing /persist we keep as-is
static void kvx_real_to_vfs_path(const char* requested_logical_dir,
                                 const char* real_in,
                                 char* out,
                                 uint32_t cap) {
    if (!out || cap == 0) return;
    out[0] = 0;

    if (!requested_logical_dir || !real_in) return;

    if (is_home_path(requested_logical_dir) || is_apps_path(requested_logical_dir)) {
        if (strncmp(real_in, "/persist", 8) == 0) {
            copy_str(out, real_in + 8, cap);
            if (out[0] == 0) copy_str(out, "/", cap);
            return;
        }
    }

    copy_str(out, real_in, cap);
}

// ==========================================================
// Path pop / cwd
// ==========================================================
static void path_pop(char* path)
{
    uint32_t len = strlen(path);

    if (len <= 1) {
        path[0] = '/';
        path[1] = 0;
        return;
    }

    if (path[len - 1] == '/') {
        path[len - 1] = 0;
        len--;
    }

    while (len > 0 && path[len - 1] != '/') {
        len--;
    }

    if (len == 0) {
        path[0] = '/';
        path[1] = 0;
    } else {
        path[len] = 0;
    }
}

// ==========================================================
// Backends
// ==========================================================
typedef enum {
    BACK_NONE = 0,
    BACK_RAM  = 1,
    BACK_TOY  = 2,
    BACK_KVX  = 3
} vfs_backend_t;

struct vfs_file {
    vfs_backend_t back;
    int           flags;

    // ram
    int           rfd;

    // toy
    int           th;

    // kvx (whole-file buffered)
    char          kvx_real_path[VFS_PATH_MAX];
    uint8_t*      kvx_buf;
    uint32_t      kvx_size;
    uint32_t      kvx_cap;
    uint32_t      kvx_pos;
    int           kvx_dirty;
};

static char vfs_cwd[128] = "/";

#ifndef VFS_MAX_OPEN
#define VFS_MAX_OPEN 32
#endif

static struct vfs_file g_open[VFS_MAX_OPEN];

static struct vfs_file* alloc_slot(void) {
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        if (g_open[i].back == BACK_NONE) return &g_open[i];
    }
    return 0;
}

static void free_slot(struct vfs_file* f) {
    if (!f) return;

    if (f->kvx_buf) {
        kfree(f->kvx_buf);
        f->kvx_buf = 0;
    }

    f->back = BACK_NONE;
    f->flags = 0;
    f->rfd = -1;
    f->th  = -1;
    f->kvx_real_path[0] = 0;
    f->kvx_size = 0;
    f->kvx_cap = 0;
    f->kvx_pos = 0;
    f->kvx_dirty = 0;
}

static int g_vfs_inited = 0;

void vfs_init(void) {
    if (g_vfs_inited) return;
    g_vfs_inited = 1;

    ramfs_init();

    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        g_open[i].back = BACK_NONE;
        g_open[i].rfd  = -1;
        g_open[i].th   = -1;
        g_open[i].kvx_buf = 0;
        g_open[i].kvx_real_path[0] = 0;
        g_open[i].kvx_size = 0;
        g_open[i].kvx_cap = 0;
        g_open[i].kvx_pos = 0;
        g_open[i].kvx_dirty = 0;
    }
}

// ==========================================================
// ToyFS API adapt
// ==========================================================
static int toy_open_ro(const char* path, int* out_h) {
    int h = toyfs_open(path);
    if (h < 0) return 0;
    *out_h = h;
    return 1;
}

static int toy_read_wrap(int h, void* out, uint32_t n, uint32_t* out_nread) {
    int r = toyfs_read(h, out, n);
    if (r < 0) return 0;
    if (out_nread) *out_nread = (uint32_t)r;
    return 1;
}

static void toy_close_wrap(int h) {
    toyfs_close(h);
}

// ==========================================================
// KVX buffered open
// ==========================================================
static int kvx_open_buffered(const char* real_path, int flags, struct vfs_file* f) {
    if (!real_path || !f) return 0;

    int want_write = (flags & VFS_O_WRONLY) || (flags & VFS_O_RDWR);
    int create = (flags & VFS_O_CREAT) ? 1 : 0;

    // dizin açma desteği yok
    if (kvx_is_dir_real(real_path)) return 0;

    uint32_t size = 0;
    int exists = kvx_file_size_real(real_path, &size);

    if (!exists && !want_write && !create) {
        return 0;
    }

    if (!exists && want_write) {
        size = 0;
    }

    uint32_t cap = (size > 0) ? size : 64;
    uint8_t* buf = (uint8_t*)kmalloc(cap + 1);
    if (!buf) return 0;
    mem_zero_local(buf, cap + 1);

    if (exists && size > 0) {
        uint32_t got = 0;
        if (!kvxfs_read_all(real_path, buf, size, &got)) {
            kfree(buf);
            return 0;
        }
        size = got;
    }

    f->back = BACK_KVX;
    f->flags = flags;
    f->rfd = -1;
    f->th = -1;
    copy_str(f->kvx_real_path, real_path, sizeof(f->kvx_real_path));
    f->kvx_buf = buf;
    f->kvx_size = size;
    f->kvx_cap = cap;
    f->kvx_pos = 0;
    f->kvx_dirty = (!exists && want_write) ? 1 : 0;

    return 1;
}

static int kvx_grow_buffer(struct vfs_file* f, uint32_t need_cap) {
    if (!f || f->back != BACK_KVX) return 0;
    if (need_cap <= f->kvx_cap) return 1;

    uint32_t newcap = f->kvx_cap ? f->kvx_cap : 64;
    while (newcap < need_cap) newcap *= 2;

    uint8_t* nb = (uint8_t*)kmalloc(newcap + 1);
    if (!nb) return 0;
    mem_zero_local(nb, newcap + 1);

    if (f->kvx_buf && f->kvx_size > 0) {
        memcpy(nb, f->kvx_buf, f->kvx_size);
    }
    if (f->kvx_buf) kfree(f->kvx_buf);

    f->kvx_buf = nb;
    f->kvx_cap = newcap;
    return 1;
}

// ==========================================================
// vfs_open
// ==========================================================
int vfs_open(const char* path, int flags, vfs_file_t** out) {
    if (!path || !out) return 0;

    char resolved[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    // /removable -> ToyFS only (read-only mount view)
    if (is_removable_path(resolved)) {
        int want_write2 = (flags & VFS_O_WRONLY) || (flags & VFS_O_RDWR);
        if (want_write2) return 0;

        char real[VFS_PATH_MAX];
        removable_to_toy(resolved, real, sizeof(real));

        int th;
        if (!toy_open_ro(real, &th)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { toy_close_wrap(th); return 0; }

        f->back  = BACK_TOY;
        f->flags = flags;
        f->rfd   = -1;
        f->th    = th;
        f->kvx_buf = 0;
        f->kvx_real_path[0] = 0;
        f->kvx_size = 0;
        f->kvx_cap = 0;
        f->kvx_pos = 0;
        f->kvx_dirty = 0;

        *out = f;
        return 1;
    }

    // /home, /apps, /persist -> KVXFS
    if (is_kvx_path(resolved)) {
        char real[VFS_PATH_MAX];
        vfs_to_kvx_path(resolved, real, sizeof(real));

        struct vfs_file* f = alloc_slot();
        if (!f) return 0;
        free_slot(f);

        if (!kvx_open_buffered(real, flags, f)) {
            free_slot(f);
            return 0;
        }

        *out = f;
        return 1;
    }

    // write -> RAMFS
    int want_write = (flags & VFS_O_WRONLY) || (flags & VFS_O_RDWR);
    if (want_write) {
        int create = (flags & VFS_O_CREAT) ? 1 : 0;
        int rfd;
        if (!ramfs_open(resolved, create, &rfd)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { ramfs_close(rfd); return 0; }

        f->back = BACK_RAM;
        f->flags = flags;
        f->rfd = rfd;
        f->th = -1;
        f->kvx_buf = 0;
        f->kvx_real_path[0] = 0;
        f->kvx_size = 0;
        f->kvx_cap = 0;
        f->kvx_pos = 0;
        f->kvx_dirty = 0;

        *out = f;
        return 1;
    }

    // read -> overlay: RAM first, then TOY
    if (ramfs_exists(resolved)) {
        int rfd;
        if (!ramfs_open(resolved, 0, &rfd)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { ramfs_close(rfd); return 0; }

        f->back = BACK_RAM;
        f->flags = flags;
        f->rfd = rfd;
        f->th = -1;
        f->kvx_buf = 0;
        f->kvx_real_path[0] = 0;
        f->kvx_size = 0;
        f->kvx_cap = 0;
        f->kvx_pos = 0;
        f->kvx_dirty = 0;

        *out = f;
        return 1;
    }

    int th;
    if (!toy_open_ro(resolved, &th)) return 0;

    struct vfs_file* f = alloc_slot();
    if (!f) { toy_close_wrap(th); return 0; }

    f->back = BACK_TOY;
    f->flags = flags;
    f->rfd = -1;
    f->th = th;
    f->kvx_buf = 0;
    f->kvx_real_path[0] = 0;
    f->kvx_size = 0;
    f->kvx_cap = 0;
    f->kvx_pos = 0;
    f->kvx_dirty = 0;

    *out = f;
    return 1;
}

// ==========================================================
// read / write
// ==========================================================
int vfs_read(vfs_file_t* f, void* out, uint32_t n, uint32_t* out_nread) {
    if (out_nread) *out_nread = 0;
    if (!f || f->back == BACK_NONE) return 0;

    if (f->back == BACK_RAM) {
        return ramfs_read(f->rfd, out, n, out_nread);
    }

    if (f->back == BACK_TOY) {
        return toy_read_wrap(f->th, out, n, out_nread);
    }

    if (f->back == BACK_KVX) {
        if (!out) return 0;
        if (f->kvx_pos >= f->kvx_size) {
            if (out_nread) *out_nread = 0;
            return 1;
        }

        uint32_t can = f->kvx_size - f->kvx_pos;
        if (can > n) can = n;

        memcpy(out, f->kvx_buf + f->kvx_pos, can);
        f->kvx_pos += can;

        if (out_nread) *out_nread = can;
        return 1;
    }

    return 0;
}

int vfs_write(vfs_file_t* f, const void* in, uint32_t n, uint32_t* out_nwritten) {
    if (out_nwritten) *out_nwritten = 0;
    if (!f || f->back == BACK_NONE) return 0;

    if (f->back == BACK_RAM) {
        return ramfs_write(f->rfd, in, n, out_nwritten);
    }

    if (f->back == BACK_TOY) {
        return 0; // read-only
    }

    if (f->back == BACK_KVX) {
        if (!in && n != 0) return 0;

        uint32_t need = f->kvx_pos + n;
        if (!kvx_grow_buffer(f, need)) return 0;

        if (n > 0) {
            memcpy(f->kvx_buf + f->kvx_pos, in, n);
            f->kvx_pos += n;
            if (f->kvx_pos > f->kvx_size) f->kvx_size = f->kvx_pos;
        }

        f->kvx_dirty = 1;
        if (out_nwritten) *out_nwritten = n;
        return 1;
    }

    return 0;
}

// ==========================================================
// mkdir
// ==========================================================
int vfs_mkdir(const char* path) {
    char resolved[VFS_PATH_MAX];
    char real[VFS_PATH_MAX];

    if (!path) return 0;
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    if (is_kvx_path(resolved)) {
        vfs_to_kvx_path(resolved, real, sizeof(real));
        return (kvxfs_mkdir(real) == 0);
    }

    return ramfs_mkdir(resolved);
}

// ==========================================================
// stat
// ==========================================================
int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!st) return 0;
    st->type = 0;
    st->size = 0;
    st->backend = 0;

    char resolved[VFS_PATH_MAX];
    char real[VFS_PATH_MAX];

    if (!path || !vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    // synthetic root entries
    if (strcmp(resolved, "/") == 0 ||
        strcmp(resolved, "/home") == 0 ||
        strcmp(resolved, "/apps") == 0 ||
        strcmp(resolved, "/tmp") == 0 ||
        strcmp(resolved, "/persist") == 0 ||
        strcmp(resolved, "/removable") == 0) {
        st->type = VFS_T_DIR;
        st->backend = 0;
        return 1;
    }

    // /removable -> toyfs view
    if (is_removable_path(resolved)) {
        char toy_real[VFS_PATH_MAX];
        removable_to_toy(resolved, toy_real, sizeof(toy_real));

        if (toyfs_iter(toy_real, 0, 0)) {
            st->type = VFS_T_DIR;
            st->backend = BACK_TOY;
            return 1;
        }

        int h = toyfs_open(toy_real);
        if (h >= 0) {
            toyfs_close(h);
            st->type = VFS_T_FILE;
            st->backend = BACK_TOY;
            return 1;
        }
        return 0;
    }

    // KVXFS area
    if (is_kvx_path(resolved)) {
        vfs_to_kvx_path(resolved, real, sizeof(real));

        if (kvx_is_dir_real(real)) {
            st->type = VFS_T_DIR;
            st->backend = BACK_KVX;
            return 1;
        }

        uint32_t sz = 0;
        if (kvx_file_size_real(real, &sz)) {
            st->type = VFS_T_FILE;
            st->size = sz;
            st->backend = BACK_KVX;
            return 1;
        }

        return 0;
    }

    if (ramfs_is_dir(resolved)) {
        st->type = VFS_T_DIR;
        st->backend = BACK_RAM;
        return 1;
    }

    if (ramfs_exists(resolved)) {
        st->type = VFS_T_FILE;
        st->backend = BACK_RAM;
        return 1;
    }

    int h = toyfs_open(resolved);
    if (h >= 0) {
        toyfs_close(h);
        st->type = VFS_T_FILE;
        st->backend = BACK_TOY;
        return 1;
    }

    if (toyfs_iter(resolved, 0, 0)) {
        st->type = VFS_T_DIR;
        st->backend = BACK_TOY;
        return 1;
    }

    return 0;
}

// ==========================================================
// cwd
// ==========================================================
const char* vfs_get_cwd(void) {
    return vfs_cwd;
}

static void copy_str(char* dst, const char* src, uint32_t cap) {
    if (!dst || cap == 0) return;
    uint32_t i = 0;
    while (src && src[i] && i + 1 < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

int vfs_set_cwd(const char* path)
{
    char new_cwd[VFS_PATH_MAX];

    if (!path || !path[0])
        return 0;

    if (strcmp(path, "..") == 0) {
        strcpy(new_cwd, vfs_get_cwd());
        path_pop(new_cwd);
        strcpy(vfs_cwd, new_cwd);
        return 1;
    }

    if (!vfs_resolve_path(path, new_cwd, sizeof(new_cwd)))
        return 0;

    if (!vfs_list(new_cwd, 0, 0))
        return 0;

    strcpy(vfs_cwd, new_cwd);
    return 1;
}

// ==========================================================
// close
// ==========================================================
void vfs_close(vfs_file_t* f) {
    if (!f || f->back == BACK_NONE) return;

    if (f->back == BACK_RAM) {
        ramfs_close(f->rfd);
        free_slot(f);
        return;
    }

    if (f->back == BACK_TOY) {
        toy_close_wrap(f->th);
        free_slot(f);
        return;
    }

    if (f->back == BACK_KVX) {
        if (f->kvx_dirty) {
            kvxfs_write_all(f->kvx_real_path, f->kvx_buf, f->kvx_size);
        }
        free_slot(f);
        return;
    }

    free_slot(f);
}

// ==========================================================
// read_all / write_all
// ==========================================================
int vfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (out_size) *out_size = 0;
    if (!path || !out) return 0;

    char resolved[VFS_PATH_MAX];
    char real[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    if (is_kvx_path(resolved)) {
        vfs_to_kvx_path(resolved, real, sizeof(real));
        return kvxfs_read_all(real, out, cap, out_size);
    }

    vfs_file_t* f = 0;
    if (!vfs_open(resolved, VFS_O_RDONLY, &f)) return 0;

    uint32_t total = 0;
    while (total < cap) {
        uint32_t got = 0;
        if (!vfs_read(f, out + total, cap - total, &got)) break;
        if (got == 0) break;
        total += got;
    }

    vfs_close(f);
    if (out_size) *out_size = total;
    return 1;
}

int vfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || (!data && size != 0)) return 0;

    char resolved[VFS_PATH_MAX];
    char real[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    if (is_kvx_path(resolved)) {
        vfs_to_kvx_path(resolved, real, sizeof(real));
        return kvxfs_write_all(real, data, size);
    }

    return ramfs_write_all(resolved, data, size);
}

// ==========================================================
// list
// ==========================================================
typedef struct {
    const char* dir;
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} vfs_list_wrap_t;

static int vfs_toyfs_iter_cb(const char* path, uint32_t size, void* u)
{
    vfs_list_wrap_t* w = (vfs_list_wrap_t*)u;
    if (!w || !w->cb) return 0;

    if (strcmp(path, w->dir) == 0)
        return 1;

    if (ramfs_exists(path))
        return 1;

    return w->cb(path, size, w->u);
}

typedef struct {
    const char* logical_dir;
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} kvx_list_wrap_t;

static int kvx_list_cb_convert(const char* real_path, uint32_t size, void* u) {
    kvx_list_wrap_t* w = (kvx_list_wrap_t*)u;
    if (!w || !w->cb) return 0;

    char outp[VFS_PATH_MAX];
    kvx_real_to_vfs_path(w->logical_dir, real_path, outp, sizeof(outp));

    return w->cb(outp, size, w->u);
}

static int vfs_list_root(int (*cb)(const char* path, uint32_t size, void* u), void* u) {
    if (!cb) return 1;
    if (!cb("/tmp", 0xFFFFFFFFU, u)) return 0;
    if (!cb("/persist", 0xFFFFFFFFU, u)) return 0;
    if (!cb("/removable", 0xFFFFFFFFU, u)) return 0;
    return 1;
}

int vfs_list(const char* dir_prefix,
             int (*cb)(const char* path, uint32_t size, void* u),
             void* u)
{
    char resolved[VFS_PATH_MAX];

    if (!dir_prefix || !dir_prefix[0]) {
        strcpy(resolved, vfs_get_cwd());
    } else {
        if (!vfs_resolve_path(dir_prefix, resolved, sizeof(resolved))) return 0;
    }

    // root synthetic view
    if (strcmp(resolved, "/") == 0) {
        if (!cb) return 1;
        return vfs_list_root(cb, u);
    }

    // /removable
    if (is_removable_path(resolved)) {
        char real[VFS_PATH_MAX];
        removable_to_toy(resolved, real, sizeof(real));

        if (!cb) {
            if (toyfs_iter(real, 0, 0)) return 1;
            return 0;
        }

        rem_wrap_t rw = { cb, u };
        toyfs_iter(real, rem_cb_prefix, &rw);
        return 1;
    }

    // KVXFS
    if (is_kvx_path(resolved)) {
        char real[VFS_PATH_MAX];
        vfs_to_kvx_path(resolved, real, sizeof(real));

        if (!cb) {
            if (strcmp(resolved, "/home") == 0 ||
                strcmp(resolved, "/apps") == 0 ||
                strcmp(resolved, "/persist") == 0)
                return 1;

            return kvx_is_dir_real(real);
        }

        kvx_list_wrap_t kw = { resolved, cb, u };
        return kvx_iter_real(real, kvx_list_cb_convert, &kw);
    }

    // only existence check
    if (!cb) {
        if (ramfs_is_dir(resolved))
            return 1;
        if (toyfs_iter(resolved, 0, 0))
            return 1;
        return 0;
    }

    ramfs_list(resolved, cb, u);

    vfs_list_wrap_t w = { resolved, cb, u };
    toyfs_iter(resolved, vfs_toyfs_iter_cb, &w);

    return 1;
}

// ==========================================================
// cd parent
// ==========================================================
void vfs_cd_parent(void)
{
    const char *cwd = vfs_get_cwd();

    if (strcmp(cwd, "/") == 0)
        return;

    char new_cwd[VFS_PATH_MAX];
    strcpy(new_cwd, cwd);

    path_pop(new_cwd);
    vfs_set_cwd(new_cwd);
}

// ==========================================================
// resolve path
// ==========================================================
int vfs_resolve_path(const char* in, char* out, uint32_t cap)
{
    if (!in || !out || cap == 0)
        return 0;

    out[0] = 0;

    // absolute
    if (in[0] == '/') {
        copy_str(out, in, cap);
        return 1;
    }

    // relative = cwd + "/" + in
    copy_str(out, vfs_get_cwd(), cap);

    if (strcmp(out, "/") != 0) {
        uint32_t len = strlen(out);
        if (len + 1 < cap) {
            out[len] = '/';
            out[len + 1] = 0;
        }
    }

    uint32_t len = strlen(out);
    copy_str(out + len, in, cap - len);

    return 1;
}

// ==========================================================
// remove / rename
// ==========================================================
int vfs_remove(const char* path) {
    if (!path) return 0;

    char resolved[VFS_PATH_MAX];
    char real[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    if (is_kvx_path(resolved)) {
        vfs_to_kvx_path(resolved, real, sizeof(real));
        return kvx_remove_real(real);
    }

    if (ramfs_exists(resolved) || ramfs_is_dir(resolved)) {
        return ramfs_remove(resolved);
    }

    return 0; // ToyFS read-only
}

int vfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return 0;

    char res_old[VFS_PATH_MAX];
    char res_new[VFS_PATH_MAX];
    char real_old[VFS_PATH_MAX];
    char real_new[VFS_PATH_MAX];

    if (!vfs_resolve_path(old_path, res_old, sizeof(res_old))) return 0;
    if (!vfs_resolve_path(new_path, res_new, sizeof(res_new))) return 0;

    // KVX -> KVX
    if (is_kvx_path(res_old) && is_kvx_path(res_new)) {
        vfs_to_kvx_path(res_old, real_old, sizeof(real_old));
        vfs_to_kvx_path(res_new, real_new, sizeof(real_new));
        return kvx_rename_real(real_old, real_new);
    }

    // RAM -> RAM
    if ((ramfs_exists(res_old) || ramfs_is_dir(res_old)) &&
        !(is_kvx_path(res_new) || is_removable_path(res_new))) {
        return ramfs_rename(res_old, res_new);
    }

    // cross-backend rename yok
    return 0;
}

// ==========================================================
// read_all_alloc
// ==========================================================
int vfs_read_all_alloc(const char* path, uint8_t** out_buf, uint32_t* out_size) {
    if (out_size) *out_size = 0;
    if (out_buf) *out_buf = 0;
    if (!path || !out_buf) return 0;

    vfs_file_t* f = 0;
    if (!vfs_open(path, VFS_O_RDONLY, &f)) return 0;

    uint32_t cap = 1024;
    uint8_t* buf = (uint8_t*)kmalloc(cap + 1);
    if (!buf) { vfs_close(f); return 0; }

    uint32_t total = 0;

    while (1) {
        if (total == cap) {
            uint32_t newcap = cap * 2;
            uint8_t* nb = (uint8_t*)kmalloc(newcap + 1);
            if (!nb) break;
            memcpy(nb, buf, cap);
            kfree(buf);
            buf = nb;
            cap = newcap;
        }

        uint32_t got = 0;
        if (!vfs_read(f, buf + total, cap - total, &got)) break;
        if (got == 0) break;
        total += got;
    }

    vfs_close(f);

    buf[total] = 0;
    *out_buf = buf;
    if (out_size) *out_size = total;
    return 1;
}

void vfs_free_alloc(void* p) {
    if (p) kfree(p);
}