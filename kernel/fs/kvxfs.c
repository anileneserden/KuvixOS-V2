#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <arch/x86/io.h>
#include <stdint.h>

#define KVX_MAGIC     "KVXFS1"
#define KVX_MAX_FILES 32
#define KVX_META_LBA  2048
#define KVX_DATA_LBA  2100
#define KVX_DIR_SIZE  0xFFFFFFFFu

typedef struct {
    char     path[64];
    uint32_t start_lba;
    uint32_t size;      // KVX_DIR_SIZE => directory
    uint8_t  used;
    uint8_t  _pad[3];
} __attribute__((packed)) kvx_ent_t;

typedef struct {
    char     magic[8];
    uint32_t file_count;
    uint32_t next_free_lba;
    kvx_ent_t ent[KVX_MAX_FILES];
} __attribute__((packed)) kvx_meta_t;

/* sizeof(kvx_meta_t) = 4 sektörü geçebilir; dinamik hesapla */
#define KVX_META_BYTES   ((uint32_t)sizeof(kvx_meta_t))
#define KVX_META_SECTORS ((KVX_META_BYTES + 511u) / 512u)

static kvx_meta_t g_meta;
static int g_inited = 0;
static uint8_t g_io_buf[KVX_META_SECTORS * 512];

static void mem_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

static int is_persist_path(const char* path) {
    if (!path) return 0;
    return strncmp(path, "/persist", 8) == 0;
}

static int is_exact_or_child_of(const char* parent, const char* path) {
    if (!parent || !path) return 0;

    int plen = strlen(parent);
    if (strncmp(parent, path, plen) != 0) return 0;

    if (path[plen] == 0) return 1;
    if (parent[plen - 1] == '/') return 1;
    return path[plen] == '/';
}

static int find_ent(const char* path) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

static int kvxfs_parent_path(const char* path, char* out, int out_sz) {
    if (!path || !out || out_sz <= 0) return 0;

    int len = strlen(path);
    if (len <= 0) return 0;

    if (len >= out_sz) len = out_sz - 1;

    strncpy(out, path, len);
    out[len] = 0;

    /* Sondaki slash'lari temizle */
    while (len > 1 && out[len - 1] == '/') {
        out[len - 1] = 0;
        len--;
    }

    /* Son slash'i bul */
    int last_slash = -1;
    for (int i = 0; out[i]; i++) {
        if (out[i] == '/') last_slash = i;
    }

    if (last_slash < 0) return 0;

    /* root parent */
    if (last_slash == 0) {
        out[1] = 0;
        return 1;
    }

    out[last_slash] = 0;
    return 1;
}

static int kvxfs_dir_exists(const char* path) {
    if (!path) return 0;

    /* /persist kokunu dizin gibi kabul et */
    if (strcmp(path, "/persist") == 0 || strcmp(path, "/persist/") == 0) {
        return 1;
    }

    int idx = find_ent(path);
    if (idx < 0) return 0;

    return g_meta.ent[idx].used && g_meta.ent[idx].size == KVX_DIR_SIZE;
}

static int alloc_ent(void) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used == 0) {
            return i;
        }
    }
    return -1;
}

static int kvxfs_is_dir_ent(const kvx_ent_t* ent) {
    return ent && ent->used && ent->size == KVX_DIR_SIZE;
}

static void kvxfs_trim_trailing_slash(const char* in, char* out, int out_sz) {
    if (!in || !out || out_sz <= 0) return;

    strncpy(out, in, out_sz - 1);
    out[out_sz - 1] = 0;

    int len = strlen(out);
    while (len > 1 && out[len - 1] == '/') {
        out[len - 1] = 0;
        len--;
    }
}

static const char* kvxfs_basename_ptr(const char* path) {
    if (!path) return "";
    const char* last = path;
    const char* p = path;
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return last;
}

static void kvxfs_print_indent(int depth) {
    for (int i = 0; i < depth; i++) {
        printk("  ");
    }
}

static int kvxfs_path_is_direct_child(const char* parent, const char* child) {
    if (!parent || !child) return 0;

    int parent_len = strlen(parent);
    if (strncmp(child, parent, parent_len) != 0) return 0;

    if (child[parent_len] == 0) return 0;
    if (child[parent_len] != '/') return 0;

    const char* rest = child + parent_len + 1;
    if (*rest == 0) return 0;

    while (*rest) {
        if (*rest == '/') return 0;
        rest++;
    }

    return 1;
}

static int meta_read(void) {
    mem_zero(g_io_buf, sizeof(g_io_buf));
    mem_zero(&g_meta, sizeof(g_meta));

    if (!block_read(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) {
        return 0;
    }

    memcpy(&g_meta, g_io_buf, sizeof(g_meta));
    return 1;
}

static int meta_write(void) {
    mem_zero(g_io_buf, sizeof(g_io_buf));
    memcpy(g_io_buf, &g_meta, sizeof(g_meta));

    for (volatile int i = 0; i < 30000; i++) io_wait();

    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) {
        return 0;
    }

    for (volatile int i = 0; i < 50000; i++) io_wait();
    return 1;
}

int kvxfs_init(void) {
    if (g_inited) return 1;

    if (!ata_pio_is_ready()) {
        printk("[KVXFS] ATA not ready\n");
        return 0;
    }

    printk("[KVXFS] reading meta lba=%d sectors=%d bytes=%d\n",
           KVX_META_LBA, KVX_META_SECTORS, KVX_META_BYTES);

    if (!meta_read()) {
        printk("[KVXFS] meta_read FAILED\n");
        return 0;
    }

    printk("[KVXFS] magic bytes: %x %x %x %x %x %x\n",
        (unsigned char)g_meta.magic[0],
        (unsigned char)g_meta.magic[1],
        (unsigned char)g_meta.magic[2],
        (unsigned char)g_meta.magic[3],
        (unsigned char)g_meta.magic[4],
        (unsigned char)g_meta.magic[5]);

    printk("[KVXFS] expect bytes: %x %x %x %x %x %x\n",
        (unsigned char)KVX_MAGIC[0],
        (unsigned char)KVX_MAGIC[1],
        (unsigned char)KVX_MAGIC[2],
        (unsigned char)KVX_MAGIC[3],
        (unsigned char)KVX_MAGIC[4],
        (unsigned char)KVX_MAGIC[5]);

    if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) {
        printk("[KVXFS] magic mismatch\n");
        return 0;
    }

    g_inited = 1;
    printk("[KVXFS] OK\n");
    return 1;
}

int kvxfs_format(void) {
    if (!ata_pio_is_ready()) return 0;

    mem_zero(&g_meta, sizeof(g_meta));

    memcpy(g_meta.magic, KVX_MAGIC, 6);
    g_meta.magic[6] = 0;
    g_meta.magic[7] = 0;

    g_meta.file_count = 0;
    g_meta.next_free_lba = KVX_DATA_LBA;

    if (!meta_write()) {
        printk("[KVXFS] format: meta_write FAILED\n");
        return 0;
    }

    /* Yazdıktan sonra gerçekten okunuyor mu doğrula */
    g_inited = 0;
    mem_zero(&g_meta, sizeof(g_meta));

    if (!meta_read()) {
        printk("[KVXFS] format: verify meta_read FAILED\n");
        return 0;
    }

    if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) {
        printk("[KVXFS] format: verify magic mismatch\n");
        return 0;
    }

    g_inited = 1;
    printk("[KVXFS] format OK\n");
    return 1;
}

int kvxfs_force_format(void) {
    g_inited = 0;
    return kvxfs_format();
}

int kvxfs_exists(const char* path) {
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    return find_ent(path) >= 0;
}

int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !data || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;

    char parent[64];
    if (!kvxfs_parent_path(path, parent, sizeof(parent))) {
        printk("[KVXFS] write: parent path parse failed: %s\n", path);
        return 0;
    }

    if (!kvxfs_dir_exists(parent)) {
        printk("[KVXFS] write: parent dir missing: %s\n", parent);
        return 0;
    }

    int idx = find_ent(path);
    if (idx < 0) {
        idx = alloc_ent();
        if (idx < 0) return 0;

        mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
        strncpy(g_meta.ent[idx].path, path, 63);
        g_meta.ent[idx].path[63] = 0;
        g_meta.ent[idx].used = 1;
        g_meta.file_count++;
    }

    /* Dizin üstüne yazma engeli */
    if (g_meta.ent[idx].size == KVX_DIR_SIZE) {
        return 0;
    }

    uint32_t start;
    uint32_t old_sectors = 0;
    uint32_t new_sectors = (size + 511u) / 512u;

    if (g_meta.ent[idx].start_lba != 0 && g_meta.ent[idx].size != KVX_DIR_SIZE) {
        old_sectors = (g_meta.ent[idx].size + 511u) / 512u;
    }

    /* Aynı ya da daha küçük dosyada mevcut alanı yeniden kullan */
    if (g_meta.ent[idx].start_lba != 0 && new_sectors <= old_sectors) {
        start = g_meta.ent[idx].start_lba;
    } else {
        start = g_meta.next_free_lba;
        g_meta.next_free_lba += new_sectors;
    }

    uint8_t sec[512];
    for (uint32_t s = 0; s < new_sectors; s++) {
        mem_zero(sec, sizeof(sec));

        uint32_t offset = s * 512u;
        uint32_t remain = (size > offset) ? (size - offset) : 0;
        uint32_t take = (remain > 512u) ? 512u : remain;

        if (take > 0) {
            memcpy(sec, data + offset, take);
        }

        if (!block_write(start + s, 1, sec)) {
            return 0;
        }

        for (volatile int i = 0; i < 5000; i++) io_wait();
    }

    g_meta.ent[idx].start_lba = start;
    g_meta.ent[idx].size = size;

    for (volatile int i = 0; i < 10000; i++) io_wait();

    if (!meta_write()) return 0;
    return 1;
}

int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (!path || !out || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;

    int idx = find_ent(path);
    if (idx < 0) return 0;
    if (g_meta.ent[idx].size == KVX_DIR_SIZE) return 0;

    uint32_t sz = (g_meta.ent[idx].size > cap) ? cap : g_meta.ent[idx].size;
    uint32_t start = g_meta.ent[idx].start_lba;

    uint8_t sec[512];
    uint32_t sectors = (sz + 511u) / 512u;

    for (uint32_t s = 0; s < sectors; s++) {
        if (!block_read(start + s, 1, sec)) return 0;

        uint32_t offset = s * 512u;
        uint32_t remain = (sz > offset) ? (sz - offset) : 0;
        uint32_t take = (remain > 512u) ? 512u : remain;

        memcpy(out + offset, sec, take);
    }

    if (out_size) *out_size = sz;
    return 1;
}

int kvxfs_mkdir(const char* path) {
    if (!path || !is_persist_path(path)) return -1;
    if (!kvxfs_init()) return -5;

    char parent[64];
    if (!kvxfs_parent_path(path, parent, sizeof(parent))) return -6;

    if (!kvxfs_dir_exists(parent)) {
        printk("[KVXFS] mkdir: parent dir missing: %s\n", parent);
        return -7;
    }

    if (find_ent(path) >= 0) return -2;

    int idx = alloc_ent();
    if (idx < 0) return -3;

    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    strncpy(g_meta.ent[idx].path, path, 63);
    g_meta.ent[idx].path[63] = 0;
    g_meta.ent[idx].size = KVX_DIR_SIZE;
    g_meta.ent[idx].used = 1;
    g_meta.file_count++;

    for (volatile int i = 0; i < 20000; i++) io_wait();

    printk("[KVXFS] mkdir meta write (slot=%d)\n", idx);
    if (!meta_write()) {
        g_meta.ent[idx].used = 0;
        g_meta.file_count--;
        return -4;
    }

    return 0;
}

void kvxfs_list_all(const char* filter_path) {
    if (!filter_path) return;

    if (!kvxfs_init()) {
        printk("[KVXFS] list_all: init basarisiz\n");
        return;
    }

    printk("--- %s Icerigi ---\n", filter_path);

    int found = 0;
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        if (!is_exact_or_child_of(filter_path, g_meta.ent[i].path)) continue;
        if (strcmp(g_meta.ent[i].path, filter_path) == 0) continue;

        if (g_meta.ent[i].size == KVX_DIR_SIZE) {
            printk("[DIR]  %s\n", g_meta.ent[i].path);
        } else {
            printk("%d byte  %s\n", g_meta.ent[i].size, g_meta.ent[i].path);
        }
        found++;
    }

    if (found == 0) {
        printk("(Dizin bos veya dosya bulunamadi)\n");
    }
}

static void kvxfs_tree_walk(const char* root_path, int depth) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        const char* path = g_meta.ent[i].path;
        if (!kvxfs_path_is_direct_child(root_path, path)) continue;

        const char* name = kvxfs_basename_ptr(path);
        kvxfs_print_indent(depth);

        if (kvxfs_is_dir_ent(&g_meta.ent[i])) {
            printk("[DIR]  %s\n", name);
            kvxfs_tree_walk(path, depth + 1);
        } else {
            printk("%d byte  %s\n", g_meta.ent[i].size, name);
        }
    }
}

static int kvxfs_has_children(const char* path) {
    if (!path) return 0;

    int plen = strlen(path);

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        const char* p = g_meta.ent[i].path;

        if (strcmp(p, path) == 0) continue;

        if (strncmp(p, path, plen) == 0 && p[plen] == '/') {
            return 1;
        }
    }

    return 0;
}

int kvxfs_tree(const char* root_path) {
    if (!root_path || !is_persist_path(root_path)) return 0;
    if (!kvxfs_init()) return 0;

    char norm[64];
    kvxfs_trim_trailing_slash(root_path, norm, sizeof(norm));

    printk("%s\n", norm);
    kvxfs_tree_walk(norm, 1);
    return 1;
}

int kvxfs_remove(const char* path) {
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;

    /* Kritik korumalar */
    if (strcmp(path, "/persist") == 0) return 0;
    if (strcmp(path, "/persist/") == 0) return 0;

    int idx = find_ent(path);
    if (idx < 0) return 0;

    /* Dizinse ve içinde çocuk varsa silme */
    if (g_meta.ent[idx].size == KVX_DIR_SIZE) {
        if (kvxfs_has_children(path)) {
            printk("[KVXFS] remove: directory not empty: %s\n", path);
            return 0;
        }
    }

    /* Slotu temizle */
    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));

    if (g_meta.file_count > 0) {
        g_meta.file_count--;
    }

    for (volatile int i = 0; i < 10000; i++) io_wait();

    if (!meta_write()) {
        printk("[KVXFS] remove: meta_write FAILED\n");
        return 0;
    }

    printk("[KVXFS] removed: %s\n", path);
    return 1;
}