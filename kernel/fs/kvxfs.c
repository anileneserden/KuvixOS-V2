#include <kernel/fs/kvxfs.h>
#include <kernel/block/block.h>
#include <kernel/drivers/ata_pio.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <arch/x86/io.h>
#include <stdint.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/video/fb_console.h>

#define KVX_MAGIC     "KVXFS1"
#define KVX_MAX_FILES 256         // Sınırı 32'den 256'ya çıkardık!
#define KVX_META_LBA  2048
#define KVX_DATA_LBA  2200         // Metadata büyüdüğü için veri başlangıcını ileriye taşıdık
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

// Sektör hesaplamalarını dinamik hale getirdik
#define KVX_META_BYTES   ((uint32_t)sizeof(kvx_meta_t))
#define KVX_META_SECTORS ((KVX_META_BYTES + 511u) / 512u)

static kvx_meta_t g_meta;
static int g_inited = 0;
// Statik buffer yerine doğrudan metadata boyutuna göre dinamik alan ayırıyoruz
static uint8_t g_io_buf[((sizeof(kvx_meta_t) + 511) / 512) * 512];

static void mem_zero(void* p, uint32_t n) {
    uint8_t* b = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) b[i] = 0;
}

static int is_persist_path(const char* path) {
    if (!path) return 0;
    if (strncmp(path, "/dev", 4) == 0) return 0;
    if (strncmp(path, "/tmp", 4) == 0) return 0;
    return 1; 
}

// Yol temizleme fonksiyonunu kusursuz hale getirdik
static void kvxfs_trim_path(const char* in, char* out, int out_sz) {
    if (!in || !out || out_sz <= 0) return;
    
    // Eğer yolun başında '/' yoksa biz ekleyelim (Standart Unix formatı)
    int offset = 0;
    if (in[0] != '/') {
        out[0] = '/';
        offset = 1;
    }

    strncpy(out + offset, in, out_sz - 1 - offset);
    out[out_sz - 1] = 0;
    
    int len = strlen(out);
    // Sondaki '/' karakterlerini temizle (Kök dizin "/" hariç)
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
    
    // Kök dizin kontrolü özel durumu
    if (strcmp(p_norm, "/") == 0) {
        const char* rest = c_norm + 1;
        while (*rest) {
            if (*rest == '/') return 0;
            rest++;
        }
        return 1;
    }

    if (strncmp(c_norm, p_norm, plen) != 0) return 0;
    if (c_norm[plen] != '/') return 0;
    
    const char* rest = c_norm + plen + 1;
    while (*rest) {
        if (*rest == '/') return 0;
        rest++;
    }
    return 1;
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
    if (!block_write(KVX_META_LBA, KVX_META_SECTORS, g_io_buf)) return 0;
    for (volatile int i = 0; i < 50000; i++) io_wait();
    return 1;
}

static void kvxfs_tree_walk(const char* root_path, int depth) {
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;

        const char* path = g_meta.ent[i].path;
        if (!kvxfs_path_is_direct_child(root_path, path)) continue;

        const char* name = kvxfs_basename_ptr(path);
        
        for (int d = 0; d < depth; d++) {
            printk("-");
        }
        printk(" "); 

        if (g_meta.ent[i].size == KVX_DIR_SIZE) {
            printk("%s/\n", name); 
            kvxfs_tree_walk(path, depth + 1);
        } else {
            printk("%s (%d byte)\n", name, g_meta.ent[i].size);
        }
    }
}

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
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    
    if (strcmp(clean, "/") == 0) return 1;
    
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
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    int idx = find_ent(clean);
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

int kvxfs_read_at(const char* path, void* out, uint32_t offset, uint32_t n, uint32_t* out_nread) {
    if (!path || !out) return 0;
    
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    int idx = find_ent(clean);
    if (idx < 0) return 0;

    uint32_t file_size = g_meta.ent[idx].size;
    if (offset >= file_size) {
        *out_nread = 0;
        return 1;
    }

    uint32_t can_read = file_size - offset;
    if (n > can_read) n = can_read;

    uint32_t start_lba = g_meta.ent[idx].start_lba;
    uint8_t sec[512];
    
    uint32_t sector_idx = offset / 512;
    uint32_t offset_in_sector = offset % 512;
    
    block_read(start_lba + sector_idx, 1, sec);
    memcpy(out, sec + offset_in_sector, n);

    *out_nread = n;
    return 1;
}

void kvxfs_list_all(const char* filter_path) {
    if (!filter_path || !kvxfs_init()) return;
    
    char norm[64];
    kvxfs_trim_path(filter_path, norm, 64);
    
    fb_console_set_color(0x00FFFFFF, 0x00000000); 
    printk("--- %s Icerigi ---\n", norm);
    
    int found = 0;
    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (!g_meta.ent[i].used) continue;
        if (strcmp(g_meta.ent[i].path, norm) == 0) continue;
        if (!kvxfs_path_is_direct_child(norm, g_meta.ent[i].path)) continue;
        
        const char* name = kvxfs_basename_ptr(g_meta.ent[i].path);
        
        if (name && name[0] == '.') {
            continue;
        }
        
        if (g_meta.ent[i].size == KVX_DIR_SIZE) {
            fb_console_set_color(0x000055FF, 0x00000000); 
            printk("%s\n", name);
        } else {
            fb_console_set_color(0x0000FF00, 0x00000000); 
            printk("%d byte  ", g_meta.ent[i].size);
            
            fb_console_set_color(0x00FFFFFF, 0x00000000); 
            printk("%s\n", name);
        }
        found++;
    }
    
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    
    if (!found) {
        printk("(Bos)\n");
    }
}

int kvxfs_tree(const char* root_path) {
    if (!root_path) return 0;
    if (!kvxfs_init()) return 0;
    
    char norm[VFS_PATH_MAX];
    kvxfs_trim_path(root_path, norm, sizeof(norm));
    
    printk("\n* (Kök Dizin: %s)\n", norm);
    kvxfs_tree_walk(norm, 1);
    printk("\n");
    return 1;
}

int kvxfs_remove(const char* path) {
    if (!path || !kvxfs_init()) return 0;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    int idx = find_ent(clean);
    if (idx < 0) return 0;
    mem_zero(&g_meta.ent[idx], sizeof(kvx_ent_t));
    g_meta.file_count--;
    return meta_write();
}

int kvxfs_exists(const char* path) {
    if (!path || !is_persist_path(path)) return 0;
    if (!kvxfs_init()) return 0;
    char clean[64];
    kvxfs_trim_path(path, clean, 64);
    return find_ent(clean) >= 0;
}

int kvxfs_open(const char* path) {
    if (!kvxfs_init()) return -1;
    return find_ent(path);
}

int kvxfs_read(int fd, void* out, uint32_t n, uint32_t* out_nread) {
    if (fd < 0 || fd >= KVX_MAX_FILES || !g_meta.ent[fd].used) return 0;
    return kvxfs_read_all(g_meta.ent[fd].path, out, n, out_nread);
}

void kvxfs_close(int fd) {
    if (fd < 0 || fd >= KVX_MAX_FILES) return;
}

int kvxfs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return 0;

    for (int i = 0; i < KVX_MAX_FILES; i++) {
        if (g_meta.ent[i].used && strcmp(g_meta.ent[i].path, old_path) == 0) {
            
            char final_new_path[64] = {0};
            strncpy(final_new_path, new_path, 63);
            
            size_t n_len = strlen(final_new_path);
            
            if ((n_len > 0 && final_new_path[n_len - 1] == '/') || kvxfs_is_dir(final_new_path)) {
                if (final_new_path[n_len - 1] != '/') {
                    strcat(final_new_path, "/");
                }
                
                const char* slash = strrchr(old_path, '/');
                if (slash) {
                    strcat(final_new_path, slash + 1);
                } else {
                    strcat(final_new_path, old_path);
                }
            }

            strncpy(g_meta.ent[i].path, final_new_path, 63);
            g_meta.ent[i].path[63] = '\0';
            
            return 1;
        }
    }
    return 0;
}