#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <kernel/drivers/ata_pio.h> // BU SATIRI EKLE
#include <lib/string.h>
#include <kernel/printk.h>
#include <arch/x86/io.h>

#define KVX_MAGIC "KVXFS1"
#define KVX_MAX_FILES 32
#define KVX_META_LBA 2048   // 8 veya 64 yerine 2048 (1. Megabaytın başı)
#define KVX_META_SECTORS 4
#define KVX_DATA_LBA 2100

typedef struct {
    char     path[64];
    uint32_t start_lba;
    uint32_t size;
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
    // 1. IO Buffer'ı tamamen sıfırla (Çöp veriyi engeller)
    mem_zero(g_io_buf, sizeof(g_io_buf));
    
    // 2. Meta yapısını buffer'ın başına kopyala
    memcpy(g_io_buf, &g_meta, sizeof(g_meta));

    // 3. Yazmadan önce donanıma nefes aldır
    for(volatile int i=0; i<30000; i++) io_wait();

    // 4. Yazma emri
    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) {
        return 0;
    }

    // 5. Yazma bittikten sonra ATA'nın kendine gelmesi için bekle
    for(volatile int i=0; i<50000; i++) io_wait();
    
    return 1;
}

static int is_persist_path(const char* path) {
    // "/persist" ile başlıyor mu?
    if (strncmp(path, "/persist", 8) == 0) return 1;
    return 0;
}

static int find_ent(const char* path) {
    for (int i=0; i<KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, path) == 0) return i;
    }
    return -1;
}

static int alloc_ent(void) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        // used sadece 0 ise bu slot gerçekten boştur
        if (g_meta.ent[i].used == 0) {
            return i;
        }
    }
    return -1;
}

int kvxfs_init(void) {
    // 1. Zaten başlatılmışsa tekrar uğraşma
    if (g_inited) return 1;
    
    // 2. ATA sürücüsünün hazır olduğundan emin ol
    if (!ata_pio_is_ready()) return 0;

    // 3. Diskten meta veriyi oku
    if (!meta_read()) {
        // Okuma hatası donanımsal bir sorundur, formatlama çözüm olmayabilir
        return 0; 
    }

    // 4. Magic Check: Disk bizim formatımızda mı?
    if (strncmp(g_meta.magic, KVX_MAGIC, 6) != 0) {
        // ÖNEMLİ: Burada otomatik format atmıyoruz! 
        // Sadece diskin "KVXFS" olmadığını bildiriyoruz.
        return 0; 
    }

    g_inited = 1;
    return 1;
}

// KVXFS'i dışarıdan zorla formatlamak için kullanılır
int kvxfs_force_format(void) {
    g_inited = 0; // Dosya içindeki static değişkene buradan erişebiliriz
    return kvxfs_init();
}

int kvxfs_write_all(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !data || !is_persist_path(path)) return 0;
    kvxfs_init();
    
    int idx = find_ent(path);
    if (idx < 0) {
        idx = alloc_ent();
        if (idx < 0) return 0;
        
        // Belleği temizleyip veriyi güvenle yazalım
        mem_zero(g_meta.ent[idx].path, 64);
        strncpy(g_meta.ent[idx].path, path, 63);
        g_meta.ent[idx].used = 1;
    }

    uint32_t start = g_meta.next_free_lba;
    uint32_t sectors = (size + 511) / 512;
    uint8_t sec[512];

    for (uint32_t s = 0; s < sectors; s++) {
        mem_zero(sec, 512);
        uint32_t take = (size - s * 512 > 512) ? 512 : (size - s * 512);
        memcpy(sec, data + (s * 512), take);
        
        // Her sektör yazımından sonra donanımın rahatlaması için mola
        if (!block_write(start + s, 1, sec)) return 0;
        for(volatile int i=0; i<5000; i++) io_wait(); 
    }

    g_meta.ent[idx].start_lba = start;
    g_meta.ent[idx].size = size;
    g_meta.next_free_lba += sectors;

    // Meta yazmadan önce son bir mola
    for(volatile int i=0; i<10000; i++) io_wait(); 
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

int kvxfs_mkdir(const char* path) {
    if (!is_persist_path(path)) return -1;
    kvxfs_init();
    
    if (find_ent(path) >= 0) return -2;

    int idx = alloc_ent();
    if (idx < 0) return -3;

    // Slot verilerini temizleyip hazırlayalım
    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    strncpy(g_meta.ent[idx].path, path, 63);
    g_meta.ent[idx].size = 0xFFFFFFFF; 
    g_meta.ent[idx].used = 1; // used bayrağını en son set etmek daha güvenlidir
    
    g_meta.file_count++;

    // Donanım önceki işlemden (mesela bir önceki mkdir) kalma komutu bitirsin
    for(volatile int i=0; i<20000; i++) io_wait(); 

    printk("DEBUG: mkdir - Meta yaziliyor (Slot: %d)...\n", idx);
    if (!meta_write()) {
        // Hata durumunda used bayrağını geri çekelim ki tablo kirlenmesin
        g_meta.ent[idx].used = 0;
        g_meta.file_count--;
        return -4;
    }

    return 0;
}

void kvxfs_list_all(const char* filter_path) {
    kvxfs_init();
    printk("--- %s Icerigi ---\n", filter_path);
    
    int filter_len = strlen(filter_path);
    int found = 0;

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used) {
            // Dosya yolu, aradığımız dizin yoluyla başlıyor mu?
            if (strncmp(g_meta.ent[i].path, filter_path, filter_len) == 0) {
                
                // Kendi yolunu listeleme (örn: /persist/system'in içinde /persist/system'i gösterme)
                if (strcmp(g_meta.ent[i].path, filter_path) == 0) continue;

                if (g_meta.ent[i].size == 0xFFFFFFFF) {
                    printk("[DIR]  %s\n", g_meta.ent[i].path);
                } else {
                    printk("%d byte  %s\n", g_meta.ent[i].size, g_meta.ent[i].path);
                }
                found++;
            }
        }
    }
    if (found == 0) printk("(Dizin bos veya dosya bulunamadi)\n");
}