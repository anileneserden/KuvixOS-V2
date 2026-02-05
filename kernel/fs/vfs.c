// src/lib/fs/vfs.c
#include <kernel/fs/vfs.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/kvxfs.h>
#include <lib/string.h>

// senin toyfs headerın:
#include <kernel/fs/toyfs.h>   // toyfs_open / toyfs_read / toyfs_close / toyfs_list

static void path_pop(char *path);

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
    int           th; // toyfs handle (int varsaydım)
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
// NOTE: toyfs API uyarlama noktası
// Burada toyfs_open(path, &handle) gibi varsaydım.
// Eğer sende farklıysa: sadece bu üç fonksiyonu düzelt.
// ----------------------------------------------------------
static int toy_open_ro(const char* path, int* out_h) {
    // örnek: int toyfs_open(const char* path); handle döner
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
        // kvxfs_read_all artık uint8_t* bekliyor, tip uyumlu oldu
        if (kvxfs_read_all(path, out, cap, out_size)) return 1;
    }

    vfs_file_t* f = 0;
    if (!vfs_open(path, VFS_O_RDONLY, &f)) return 0;

    uint32_t total = 0;
    while (total < cap) {
        uint32_t got = 0;
        // Burada zaten uint8_t* cast yapıyordun, artık gerek kalmadı ama durabilir
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
        // kvxfs_write_all artık const uint8_t* bekliyor
        if (kvxfs_write_all(path, data, size)) return 1;

        return 0;
    }

    return ramfs_write_all(path, data, size);
}

__attribute__((unused)) static int list_cb_dedupe(const char* path, uint32_t size, void* u) {
    // u = original cb + user
    struct pack { int (*cb)(const char*,uint32_t,void*); void* u; } *p = (struct pack*)u;
    return p->cb(path, size, p->u);
}

// vfs_list içinde kullanacağımız küçük wrapper
typedef struct {
    const char* dir;   // ⭐ EKLENDİ
    int (*cb)(const char* path, uint32_t size, void* u);
    void* u;
} vfs_list_wrap_t;

static int vfs_toyfs_iter_cb(const char* path, uint32_t size, void* u)
{
    vfs_list_wrap_t* w = (vfs_list_wrap_t*)u;
    if (!w || !w->cb) return 0;

    // 🔴 DİZİNİN KENDİSİNİ ATLAMAK
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

// ⭐ GÜNCELLENEN vfs_remove fonksiyonu
int vfs_remove(const char* path) {
    if (!path) return 0;

    char resolved[VFS_PATH_MAX];
    if (!vfs_resolve_path(path, resolved, sizeof(resolved))) return 0;

    // RAMFS'te mi? (Masaüstü dosyaları burada tutulur)
    if (ramfs_exists(resolved) || ramfs_is_dir(resolved)) {
        return ramfs_remove(resolved); // Yeni eklediğimiz fonksiyonu çağırıyoruz
    }

    // ToyFS salt okunurdur, silme yapılamaz
    return 0;
}