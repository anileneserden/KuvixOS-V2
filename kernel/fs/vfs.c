#include <kernel/memory/kmalloc.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/kvxfs.h>
#include <lib/string.h>

// toyfs header:
#include <kernel/fs/toyfs.h>   // toyfs_open / toyfs_read / toyfs_close / toyfs_iter

static void path_pop(char *path);

// ----------------------------------------------------------
// Removable mount view: /removable -> ToyFS root
// ----------------------------------------------------------
#define REMOUNT_PREFIX "/removable"
#define REMOUNT_PREFIX_LEN 10

static void copy_str(char* dst, const char* src, uint32_t cap);

static int is_removable_path(const char* path) {
    if (!path) return 0;
    // must be exactly "/removable" or start with "/removable/"
    if (strncmp(path, REMOUNT_PREFIX, REMOUNT_PREFIX_LEN) != 0) return 0;
    return (path[REMOUNT_PREFIX_LEN] == 0 || path[REMOUNT_PREFIX_LEN] == '/');
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

    // p starts with '/'
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

    // "/removable"
    copy_str(outp, REMOUNT_PREFIX, sizeof(outp));

    // toy_path "/" ise root'u temsil ediyor olabilir, onu atlayabiliriz.
    // (vfs_list tarafında zaten parent skip yapıyorsun ama burada da güvenli tutalım)
    if (toy_path && strcmp(toy_path, "/") != 0) {
        // toy_path absolute başlar: "/system/.."
        strncat(outp, toy_path, sizeof(outp) - strlen(outp) - 1);
    }

    return w->cb(outp, size, w->u);
}

// ----------------------------------------------------------

static void path_pop(char* path)
{
    uint32_t len = strlen(path);

    // root ise çıkma
    if (len <= 1) {
        path[0] = '/';
        path[1] = 0;
        return;
    }

    // sondaki '/' varsa sil
    if (path[len - 1] == '/') {
        path[len - 1] = 0;
        len--;
    }

    // son '/' bul
    while (len > 0 && path[len - 1] != '/') {
        len--;
    }

    // root'a kadar geldiysek
    if (len == 0) {
        path[0] = '/';
        path[1] = 0;
    } else {
        path[len] = 0;
    }
}

typedef enum {
    BACK_RAM = 1,
    BACK_TOY = 2
} vfs_backend_t;

struct vfs_file {
    vfs_backend_t back;
    int           flags;
    // ram
    int           rfd;
    // toy
    int           th; // toyfs handle
};

static char vfs_cwd[128] = "/";

#ifndef VFS_MAX_OPEN
#define VFS_MAX_OPEN 32
#endif

static struct vfs_file g_open[VFS_MAX_OPEN];

static struct vfs_file* alloc_slot(void) {
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        if (g_open[i].back == 0) return &g_open[i];
    }
    return 0;
}

static void free_slot(struct vfs_file* f) {
    if (!f) return;
    f->back = 0;
    f->flags = 0;
    f->rfd = -1;
    f->th  = -1;
}

static int g_vfs_inited = 0;

void vfs_init(void) {
    if (g_vfs_inited) return;
    g_vfs_inited = 1;

    ramfs_init();

    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        g_open[i].back =  0;
        g_open[i].rfd  = -1;
        g_open[i].th   = -1;
    }
}

// ----------------------------------------------------------
// ToyFS API adapt
// ----------------------------------------------------------
static int toy_open_ro(const char* path, int* out_h) {
    int h = toyfs_open(path);
    if (h < 0) return 0;
    *out_h = h;
    return 1;
}

static int toy_read(int h, void* out, uint32_t n, uint32_t* out_nread) {
    int r = toyfs_read(h, out, n);
    if (r < 0) return 0;
    if (out_nread) *out_nread = (uint32_t)r;
    return 1;
}

static void toy_close(int h) {
    toyfs_close(h);
}
// ----------------------------------------------------------

int vfs_open(const char* path, int flags, vfs_file_t** out) {
    if (!path || !out) return 0;

    // ------------------------------------------------------
    // /removable -> ToyFS only (read-only mount view)
    // ------------------------------------------------------
    if (is_removable_path(path)) {
        int want_write2 = (flags & VFS_O_WRONLY) || (flags & VFS_O_RDWR);
        if (want_write2) return 0;

        char real[VFS_PATH_MAX];
        removable_to_toy(path, real, sizeof(real));

        int th;
        if (!toy_open_ro(real, &th)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { toy_close(th); return 0; }

        f->back  = BACK_TOY;
        f->flags = flags;
        f->rfd   = -1;
        f->th    = th;
        *out = f;
        return 1;
    }

    // write -> always RAM
    int want_write = (flags & VFS_O_WRONLY) || (flags & VFS_O_RDWR);
    if (want_write) {
        int create = (flags & VFS_O_CREAT) ? 1 : 0;
        int rfd;
        if (!ramfs_open(path, create, &rfd)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { ramfs_close(rfd); return 0; }

        f->back = BACK_RAM;
        f->flags = flags;
        f->rfd = rfd;
        f->th = -1;
        *out = f;
        return 1;
    }

    // read -> overlay: RAM first, then TOY
    if (ramfs_exists(path)) {
        int rfd;
        if (!ramfs_open(path, 0, &rfd)) return 0;

        struct vfs_file* f = alloc_slot();
        if (!f) { ramfs_close(rfd); return 0; }

        f->back = BACK_RAM;
        f->flags = flags;
        f->rfd = rfd;
        f->th = -1;
        *out = f;
        return 1;
    }

    int th;
    if (!toy_open_ro(path, &th)) return 0;

    struct vfs_file* f = alloc_slot();
    if (!f) { toy_close(th); return 0; }

    f->back = BACK_TOY;
    f->flags = flags;
    f->rfd = -1;
    f->th = th;
    *out = f;
    return 1;
}

int vfs_read(vfs_file_t* f, void* out, uint32_t n, uint32_t* out_nread) {
    if (out_nread) *out_nread = 0;
    if (!f || f->back == 0) return 0;

    if (f->back == BACK_RAM) {
        return ramfs_read(f->rfd, out, n, out_nread);
    } else {
        return toy_read(f->th, out, n, out_nread);
    }
}

int vfs_write(vfs_file_t* f, const void* in, uint32_t n, uint32_t* out_nwritten) {
    if (out_nwritten) *out_nwritten = 0;
    if (!f || f->back == 0) return 0;

    // toyfs is read-only
    if (f->back != BACK_RAM) return 0;

    return ramfs_write(f->rfd, in, n, out_nwritten);
}

int vfs_mkdir(const char* path) {
    return ramfs_mkdir(path);
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (!st) return 0;
    st->type = 0; st->size = 0; st->backend = 0;

    // /removable -> toyfs view
    if (is_removable_path(path)) {
        char real[VFS_PATH_MAX];
        removable_to_toy(path, real, sizeof(real));

        if (toyfs_iter(real, 0, 0)) {
            st->type = VFS_T_DIR;
            st->backend = 2;
            return 1;
        }

        int h = toyfs_open(real);
        if (h >= 0) {
            toyfs_close(h);
            st->type = VFS_T_FILE;
            st->backend = 2;
            return 1;
        }
        return 0;
    }

    if (ramfs_is_dir(path)) {
        st->type = VFS_T_DIR;
        st->backend = 1;
        return 1;
    }

    if (ramfs_exists(path)) {
        st->type = VFS_T_FILE;
        st->backend = 1;
        return 1;
    }

    int h = toyfs_open(path);
    if (h >= 0) {
        toyfs_close(h);
        st->type = VFS_T_FILE;
        st->backend = 2;
        return 1;
    }

    return 0;
}

const char* vfs_get_cwd(void) {
    return vfs_cwd;
}

static void copy_str(char* dst, const char* src, uint32_t cap) {
    if (!dst || cap == 0) return;
    uint32_t i = 0;
    while (src && src[i] && i + 1 < cap) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

int vfs_set_cwd(const char* path)
{
    char new_cwd[VFS_PATH_MAX];

    if (!path || !path[0])
        return 0;

    // cd ..
    if (strcmp(path, "..") == 0) {
        strcpy(new_cwd, vfs_get_cwd());
        path_pop(new_cwd);
        strcpy(vfs_cwd, new_cwd);
        return 1;
    }

    // path çöz
    if (!vfs_resolve_path(path, new_cwd, sizeof(new_cwd)))
        return 0;

    // dizin mi kontrol et
    if (!ramfs_is_dir(new_cwd) && !toyfs_iter(new_cwd, 0, 0))
        return 0;

    strcpy(vfs_cwd, new_cwd);
    return 1;
}

void vfs_close(vfs_file_t* f) {
    if (!f || f->back == 0) return;
    if (f->back == BACK_RAM) ramfs_close(f->rfd);
    else toy_close(f->th);
    free_slot(f);
}

int vfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (out_size) *out_size = 0;

    if (path && path[0] == '/' && path[1] == 'p') {
        if (kvxfs_read_all(path, out, cap, out_size)) return 1;
    }

    vfs_file_t* f = 0;
    if (!vfs_open(path, VFS_O_RDONLY, &f)) return 0;

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
    if (!path || !data) return 0;

    if (path[0] == '/' && path[1] == 'p') {
        if (kvxfs_write_all(path, data, size)) return 1;
        return 0;
    }

    return ramfs_write_all(path, data, size);
}

// vfs_list içinde kullanacağımız küçük wrapper
typedef struct {
    const char* dir;
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} vfs_list_wrap_t;

static int vfs_toyfs_iter_cb(const char* path, uint32_t size, void* u)
{
    vfs_list_wrap_t* w = (vfs_list_wrap_t*)u;
    if (!w || !w->cb) return 0;

    // dizinin kendisini atla
    if (strcmp(path, w->dir) == 0)
        return 1;

    // overlay: RAM'de varsa toyfs'i gösterme
    if (ramfs_exists(path))
        return 1;

    return w->cb(path, size, w->u);
}

int vfs_list(const char* dir_prefix,
             int (*cb)(const char* path, uint32_t size, void* u),
             void* u)
{
    char resolved[VFS_PATH_MAX];

    // boş veya NULL → cwd
    if (!dir_prefix || !dir_prefix[0]) {
        strcpy(resolved, vfs_get_cwd());
    } else {
        vfs_resolve_path(dir_prefix, resolved, sizeof(resolved));
    }

    // /removable view
    if (is_removable_path(resolved)) {
        char real[VFS_PATH_MAX];
        removable_to_toy(resolved, real, sizeof(real));

        // sadece kontrol (var mı)
        if (!cb) {
            if (toyfs_iter(real, 0, 0)) return 1;
            return 0;
        }

        rem_wrap_t rw = { cb, u };
        toyfs_iter(real, rem_cb_prefix, &rw);
        return 1;
    }

    // sadece kontrol (cd için)
    if (!cb) {
        if (ramfs_is_dir(resolved))
            return 1;
        if (toyfs_iter(resolved, 0, 0))
            return 1;
        return 0;
    }

    ramfs_list(resolved, cb, u);

    vfs_list_wrap_t w = { dir_prefix, cb, u };
    toyfs_iter(resolved, vfs_toyfs_iter_cb, &w);

    return 1;
}

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

int vfs_remove(const char* path) {
    if (!path) return 0;

    char resolved[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    if (ramfs_exists(resolved) || ramfs_is_dir(resolved)) {
        return ramfs_remove(resolved);
    }

    // ToyFS read-only
    return 0;
}

int vfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return 0;

    char res_old[VFS_PATH_MAX];
    char res_new[VFS_PATH_MAX];

    if (!vfs_resolve_path(old_path, res_old, sizeof(res_old))) return 0;
    if (!vfs_resolve_path(new_path, res_new, sizeof(res_new))) return 0;

    if (ramfs_exists(res_old) || ramfs_is_dir(res_old)) {
        return ramfs_rename(res_old, res_new);
    }

    // ToyFS read-only
    return 0;
}

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