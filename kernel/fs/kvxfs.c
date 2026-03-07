#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <arch/x86/io.h>
#include <stdint.h>

#define KVX_MAGIC "KVXFS1"
#define KVX_MAX_FILES 32
#define KVX_META_LBA 2048
#define KVX_META_SECTORS 8
#define KVX_DATA_LBA 2100

typedef struct {
    char     path[64];
    uint32_t start_lba;
    uint32_t size;      // 0xFFFFFFFF => directory
    uint8_t  used;
    uint8_t  _pad[3];
} __attribute__((packed)) kvx_ent_t;

typedef struct {
    char     magic[8];
    uint32_t file_count;
    uint32_t next_free_lba;
    kvx_ent_t ent[KVX_MAX_FILES];
} __attribute__((packed)) kvx_meta_t;

static kvx_meta_t g_meta;
static int g_inited = 0;
static uint8_t g_io_buf[KVX_META_SECTORS * 512];

static void mem_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

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

    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) {
        return 0;
    }

    for (volatile int i = 0; i < 50000; i++) io_wait();
    return 1;
}

static int is_persist_path(const char* path) {
    if (!path) return 0;
    if (strncmp(path, "/persist", 8) != 0) return 0;
    return (path[8] == 0 || path[8] == '/');
}

static int find_ent(const char* path) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, path) == 0) {
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

int kvxfs_init(void) {
    if (g_inited) return 1;

    if (!ata_pio_is_ready()) {
        printk("[KVXFS] ATA not ready\n");
        return 0;
    }

    printk("[KVXFS] reading meta\n");

    if (!meta_read()) {
        printk("[KVXFS] meta_read FAILED\n");
        return 0;
    }

    printk("[KVXFS] magic bytes: ");
    for (int i = 0; i < 6; i++) {
        char c = g_meta.magic[i];
        if (c >= 32 && c <= 126) printk("%c", c);
        else printk(".");
    }
    printk("\n");

    if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) {
        printk("[KVXFS] magic mismatch\n");
        return 0;
    }

    printk("[KVXFS] OK\n");
    g_inited = 1;
    return 1;
}

int kvxfs_format(void) {
    if (!ata_pio_is_ready()) {
        printk("[KVXFS] format: ATA not ready\n");
        return 0;
    }

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

    g_inited = 1;
    printk("[KVXFS] format OK\n");
    return 1;
}

int kvxfs_force_format(void) {
    g_inited = 0;
    return kvxfs_format();
}

int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !is_persist_path(path)) return 0;
    if (size > 0 && !data) return 0;
    if (!kvxfs_init()) return 0;

    int idx = find_ent(path);
    int is_new = 0;

    if (idx < 0) {
        idx = alloc_ent();
        if (idx < 0) return 0;

        mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
        strncpy(g_meta.ent[idx].path, path, 63);
        g_meta.ent[idx].path[63] = 0;
        g_meta.ent[idx].used = 1;
        is_new = 1;
    } else {
        if (g_meta.ent[idx].size == 0xFFFFFFFFU) {
            // existing path is directory
            return 0;
        }
    }

    uint32_t start = g_meta.next_free_lba;
    uint32_t sectors = (size + 511) / 512;
    uint8_t sec[512];

    for (uint32_t s = 0; s < sectors; s++) {
        mem_zero(sec, sizeof(sec));

        uint32_t remain = size - (s * 512);
        uint32_t take = (remain > 512) ? 512 : remain;

        if (take > 0) {
            memcpy(sec, data + (s * 512), take);
        }

        if (!block_write(start + s, 1, sec)) return 0;
        for (volatile int i = 0; i < 5000; i++) io_wait();
    }

    g_meta.ent[idx].start_lba = start;
    g_meta.ent[idx].size = size;
    g_meta.next_free_lba += sectors;

    if (is_new) g_meta.file_count++;

    for (volatile int i = 0; i < 10000; i++) io_wait();
    return meta_write();
}

int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (!path || !out || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;

    int idx = find_ent(path);
    if (idx < 0) return 0;

    if (g_meta.ent[idx].size == 0xFFFFFFFFU) {
        // directory cannot be read as file
        return 0;
    }

    uint32_t sz = (g_meta.ent[idx].size > cap) ? cap : g_meta.ent[idx].size;
    uint32_t start = g_meta.ent[idx].start_lba;
    uint8_t sec[512];

    for (uint32_t s = 0; s < (sz + 511) / 512; s++) {
        if (!block_read(start + s, 1, sec)) return 0;

        uint32_t remain = sz - (s * 512);
        uint32_t take = (remain > 512) ? 512 : remain;
        memcpy(out + (s * 512), sec, take);
    }

    if (out_size) *out_size = sz;
    return 1;
}

int kvxfs_mkdir(const char* path) {
    if (!is_persist_path(path)) return -1;
    if (!kvxfs_init()) return -5;

    if (find_ent(path) >= 0) return -2;

    int idx = alloc_ent();
    if (idx < 0) return -3;

    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    strncpy(g_meta.ent[idx].path, path, 63);
    g_meta.ent[idx].path[63] = 0;
    g_meta.ent[idx].size = 0xFFFFFFFFU;
    g_meta.ent[idx].used = 1;

    g_meta.file_count++;

    for (volatile int i = 0; i < 20000; i++) io_wait();

    printk("[KVXFS] mkdir meta write\n");
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
        printk("[KVXFS] list failed: init failed\n");
        return;
    }

    printk("--- %s Icerigi ---\n", filter_path);

    int filter_len = (int)strlen(filter_path);
    int found = 0;

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        if (strncmp(g_meta.ent[i].path, filter_path, filter_len) != 0) continue;

        if (strcmp(g_meta.ent[i].path, filter_path) == 0) continue;

        if (g_meta.ent[i].size == 0xFFFFFFFFU) {
            printk("[DIR] %s\n", g_meta.ent[i].path);
        } else {
            printk("[FILE] %s\n", g_meta.ent[i].path);
        }
        found++;
    }

    if (found == 0) {
        printk("(Dizin bos veya dosya bulunamadi)\n");
    }
}