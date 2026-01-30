#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <lib/string.h>
#include <kernel/printk.h>

#define KVX_MAGIC "KVXFS1"
#define KVX_MAX_FILES 32
#define KVX_META_LBA 8
#define KVX_META_SECTORS 4
#define KVX_DATA_LBA 12

typedef struct {
    char     path[64];
    uint32_t start_lba;
    uint32_t size;
    uint8_t  used;
    uint8_t  _pad[3];
} kvx_ent_t;

typedef struct {
    char     magic[8];
    uint32_t file_count;
    uint32_t next_free_lba;
    kvx_ent_t ent[KVX_MAX_FILES];
} kvx_meta_t;

static kvx_meta_t g_meta;
static int g_inited = 0;
// Fonksiyonların dışına, dosya seviyesine (static) taşıyalım
static uint8_t g_io_buf[KVX_META_SECTORS * 512];

static void mem_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i=0; i<n; i++) b[i]=0;
}

static int meta_read(void) {
    uint8_t buf[KVX_META_SECTORS * 512];
    if (!block_read(KVX_META_LBA, KVX_META_SECTORS, buf)) return 0;
    memcpy(&g_meta, buf, sizeof(g_meta));
    return 1;
}

static int meta_write(void) {
    mem_zero(g_io_buf, sizeof(g_io_buf));
    memcpy(g_io_buf, &g_meta, sizeof(g_meta));
    // Sektör sayısının ve LBA'nın doğruluğundan emin ol
    return block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf);
}

static int is_persist_path(const char* path) {
    return (strncmp(path, "/persist/", 9) == 0);
}

static int find_ent(const char* path) {
    for (int i=0; i<KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, path) == 0) return i;
    }
    return -1;
}

static int alloc_ent(void) {
    for (int i=0; i<KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) return i;
    }
    return -1;
}

int kvxfs_init(void) {
    if (g_inited) return 1;
    
    // ATA sürücüsü identify=1 dediyse, block katmanı hazır sayılır.
    // block_has_root() kontrolün bazen geç kalabilir, o yüzden burayı biraz esnetelim.
    
    if (!meta_read()) {
        // Okuma başarısızsa veya disk boşsa: FORMATLA
        printk("KVXFS: Disk bos, formatlaniyor...\n");
        mem_zero(&g_meta, sizeof(g_meta));
        memcpy(g_meta.magic, KVX_MAGIC, 6);
        g_meta.file_count = 0;
        g_meta.next_free_lba = KVX_DATA_LBA;
        
        if (!meta_write()) {
            printk("KVXFS: HATA - Formatlanamadi!\n");
            return 0;
        }
    } else if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) {
        // Magic yanlışsa: FORMATLA
        printk("KVXFS: Gecersiz magic, yeniden formatlaniyor...\n");
        mem_zero(&g_meta, sizeof(g_meta));
        memcpy(g_meta.magic, KVX_MAGIC, 6);
        g_meta.next_free_lba = KVX_DATA_LBA;
        meta_write();
    }

    g_inited = 1;
    return 1;
}

int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !data || !is_persist_path(path)) return 0;
    kvxfs_init();
    int idx = find_ent(path);
    if (idx < 0) {
        idx = alloc_ent();
        if (idx < 0) return 0;
        strncpy(g_meta.ent[idx].path, path, 63);
        g_meta.ent[idx].used = 1;
    }
    uint32_t start = g_meta.next_free_lba;
    uint32_t sectors = (size + 511) / 512;
    uint8_t sec[512];
    for (uint32_t s=0; s<sectors; s++) {
        mem_zero(sec, 512);
        uint32_t take = (size - s*512 > 512) ? 512 : (size - s*512);
        memcpy(sec, data + (s*512), take);
        block_write(start + s, 1, sec);
    }
    g_meta.ent[idx].start_lba = start;
    g_meta.ent[idx].size = size;
    g_meta.next_free_lba += sectors;
    return meta_write();
}

int kvxfs_read_all(const char* path, uint8_t* out, uint32_t cap, uint32_t* out_size) {
    if (!path || !out || !is_persist_path(path)) return 0;
    kvxfs_init();
    int idx = find_ent(path);
    if (idx < 0) return 0;
    uint32_t sz = (g_meta.ent[idx].size > cap) ? cap : g_meta.ent[idx].size;
    uint32_t start = g_meta.ent[idx].start_lba;
    uint8_t sec[512];
    for (uint32_t s=0; s < (sz+511)/512; s++) {
        block_read(start + s, 1, sec);
        uint32_t take = (sz - s*512 > 512) ? 512 : (sz - s*512);
        memcpy(out + (s*512), sec, take);
    }
    if (out_size) *out_size = sz;
    return 1;
}