#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <arch/x86/io.h>
#include <stdint.h>
#include <kernel/fs/vfs.h>

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

#define KVX_META_BYTES   ((uint32_t)sizeof(kvx_meta_t))
#define KVX_META_SECTORS ((KVX_META_BYTES + 511u) / 512u)

static kvx_meta_t g_meta;
static int g_inited = 0;
static uint8_t g_io_buf[KVX_META_SECTORS * 512];

// --- Yardımcı Fonksiyonlar ---

static void mem_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

static int is_persist_path(const char* path) {
    if (!path) return 0;
    // Eğer yol /dev veya /tmp gibi sanal dosya sistemlerine ait değilse, 
    // varsayılan olarak diske (KVXFS) ait kabul et.
    if (strncmp(path, "/dev", 4) == 0) return 0;
    if (strncmp(path, "/tmp", 4) == 0) return 0;
    return 1; 
}

static void kvxfs_trim_path(const char* in, char* out, int out_sz) {
    if (!in || !out || out_sz <= 0) return;
    strncpy(out, in, out_sz - 1);
    out[out_sz - 1] = 0;
    int len = strlen(out);
    while (len > 1 && out[len - 1] == '/') {
        out[len - 1] = 0;
        len--;
    }
}

static int find_ent(const char* path) {
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, clean) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_ent(void) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used == 0) return i;
    }
    return -1;
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

static int kvxfs_path_is_direct_child(const char* parent, const char* child) {
    char p_norm[64], c_norm[64];
    kvxfs_trim_path(parent, p_norm, 64);
    kvxfs_trim_path(child, c_norm, 64);

    int plen = strlen(p_norm);
    if (strncmp(c_norm, p_norm, plen) != 0) return 0;
    
    const char* rest = c_norm + plen;
    if (*rest == '/') rest++;
    if (*rest == 0) return 0;
    while (*rest) {
        if (*rest == '/') return 0;
        rest++;
    }
    return 1;
}

// --- Disk IO ---

static int meta_read(void) {
    mem_zero(g_io_buf, sizeof(g_io_buf));
    if (!block_read(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) return 0;
    memcpy(&g_meta, g_io_buf, sizeof(g_meta));
    return 1;
}

static int meta_write(void) {
    mem_zero(g_io_buf, sizeof(g_io_buf));
    memcpy(g_io_buf, &g_meta, sizeof(g_meta));
    for (volatile int i = 0; i < 30000; i++) io_wait();
    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) return 0;
    for (volatile int i = 0; i < 50000; i++) io_wait();
    return 1;
}

// --- Dahili Fonksiyonlar (Ağaç yapısı vb) ---

static void kvxfs_tree_walk(const char* root_path, int depth) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        const char* path = g_meta.ent[i].path;
        if (!kvxfs_path_is_direct_child(root_path, path)) continue;

        const char* name = kvxfs_basename_ptr(path);
        for (int d = 0; d < depth; d++) printk("  ");

        if (g_meta.ent[i].size == KVX_DIR_SIZE) {
            printk("[DIR]  %s\n", name);
            kvxfs_tree_walk(path, depth + 1);
        } else {
            printk("%d byte  %s\n", g_meta.ent[i].size, name);
        }
    }
}

// --- Kamu API Fonksiyonları ---

int kvxfs_init(void) {
    if (g_inited) return 1;
    if (!ata_pio_is_ready()) return 0;
    if (!meta_read()) return 0;
    if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) return 0;
    g_inited = 1;
    return 1;
}

int kvxfs_format(void) {
    if (!ata_pio_is_ready()) return 0;
    mem_zero(&g_meta, sizeof(g_meta));
    memcpy(g_meta.magic, KVX_MAGIC, 6);
    g_meta.file_count = 0;
    g_meta.next_free_lba = KVX_DATA_LBA;
    if (!meta_write()) return 0;
    g_inited = 1;
    return 1;
}

int kvxfs_force_format(void) {
    g_inited = 0;
    return kvxfs_format();
}

int kvxfs_is_dir(const char* path) {
    printk("[KVXFS] is_dir sorgusu: '%s'\n", path); // Bunu ekle
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    if (strcmp(clean, "/persist") == 0) return 1;
    int idx = find_ent(clean);
    if (idx < 0) return 0;
    return g_meta.ent[idx].used && g_meta.ent[idx].size == KVX_DIR_SIZE;
}

int kvxfs_mkdir(const char* path) {
    if (!path || !is_persist_path(path)) return -1;
    if (!kvxfs_init()) return -5;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    if (find_ent(clean) >= 0) return -2;
    int idx = alloc_ent();
    if (idx < 0) return -3;
    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    strncpy(g_meta.ent[idx].path, clean, 63);
    g_meta.ent[idx].size = KVX_DIR_SIZE;
    g_meta.ent[idx].used = 1;
    g_meta.file_count++;
    if (!meta_write()) {
        g_meta.ent[idx].used = 0;
        g_meta.file_count--;
        return -4;
    }
    return 0;
}

int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    int idx = find_ent(clean);
    if (idx < 0) {
        idx = alloc_ent();
        if (idx < 0) return 0;
        strncpy(g_meta.ent[idx].path, clean, 63);
        g_meta.ent[idx].used = 1;
        g_meta.file_count++;
    }
    uint32_t new_sectors = (size + 511u) / 512u;
    uint32_t start = g_meta.next_free_lba;
    uint8_t sec[512];
    for (uint32_t s = 0; s < new_sectors; s++) {
        mem_zero(sec, 512);
        uint32_t take = (size - s * 512 > 512) ? 512 : (size - s * 512);
        if (data) memcpy(sec, data + (s * 512), take);
        block_write(start + s, 1, sec);
    }
    g_meta.ent[idx].start_lba = start;
    g_meta.ent[idx].size = size;
    g_meta.next_free_lba += new_sectors;
    return meta_write();
}

int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (!path || !out || !kvxfs_init()) return 0;
    int idx = find_ent(path);
    if (idx < 0 || g_meta.ent[idx].size == KVX_DIR_SIZE) return 0;
    uint32_t sz = (g_meta.ent[idx].size > cap) ? cap : g_meta.ent[idx].size;
    uint32_t sectors = (sz + 511u) / 512u;
    uint8_t sec[512];
    for (uint32_t s = 0; s < sectors; s++) {
        block_read(g_meta.ent[idx].start_lba + s, 1, sec);
        uint32_t take = (sz - s * 512 > 512) ? 512 : (sz - s * 512);
        memcpy(out + (s * 512), sec, take);
    }
    if (out_size) *out_size = sz;
    return 1;
}

void kvxfs_list_all(const char* filter_path) {
    if (!filter_path || !kvxfs_init()) return;
    char norm[64];
    kvxfs_trim_path(filter_path, norm, 64);
    printk("--- %s Icerigi ---\n", norm);
    int found = 0;
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;
        if (strcmp(g_meta.ent[i].path, norm) == 0) continue;
        if (!kvxfs_path_is_direct_child(norm, g_meta.ent[i].path)) continue;
        const char* name = kvxfs_basename_ptr(g_meta.ent[i].path);
        if (g_meta.ent[i].size == KVX_DIR_SIZE) printk("[DIR]  %s\n", name);
        else printk("%d byte  %s\n", g_meta.ent[i].size, name);
        found++;
    }
    if (!found) printk("(Bos)\n");
}

int kvxfs_tree(const char* root_path) {
    if (!root_path || !is_persist_path(root_path)) return 0;
    if (!kvxfs_init()) return 0;
    char norm[64];
    kvxfs_trim_path(root_path, norm, 64);
    printk("%s\n", norm);
    kvxfs_tree_walk(norm, 1);
    return 1;
}

int kvxfs_remove(const char* path) {
    if (!path || !kvxfs_init()) return 0;
    int idx = find_ent(path);
    if (idx < 0) return 0;
    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    g_meta.file_count--;
    return meta_write();
}

int kvxfs_exists(const char* path) {
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    return find_ent(path) >= 0;
}