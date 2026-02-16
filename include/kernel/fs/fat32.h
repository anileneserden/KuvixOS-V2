// include/kernel/fs/fat32.h
#pragma once
#include <stdint.h>
#include <kernel/block/blockdev.h>

#ifdef __cplusplus
extern "C" {
#endif

// FAT32 Dir Attr bits
#define FAT_ATTR_READONLY 0x01
#define FAT_ATTR_HIDDEN   0x02
#define FAT_ATTR_SYSTEM   0x04
#define FAT_ATTR_VOLUMEID 0x08
#define FAT_ATTR_DIR      0x10
#define FAT_ATTR_ARCHIVE  0x20
#define FAT_ATTR_LFN      0x0F  // (attr == 0x0F) -> Long File Name entry

typedef struct {
    // hangi block device üzerinde?
    blockdev_t* dev;

    // partition start LBA (MBR’den)
    uint32_t part_lba;

    // BPB (FAT32)
    uint16_t bytes_per_sector;     // genelde 512
    uint8_t  sectors_per_cluster;  // 1,2,4,8...
    uint16_t reserved_sectors;     // genelde 32
    uint8_t  num_fats;             // genelde 2
    uint32_t fat_size_sectors;     // FAT32 size (sectors)
    uint32_t root_cluster;         // genelde 2

    // türetilenler
    uint32_t fat_lba;              // part_lba + reserved
    uint32_t data_lba;             // part_lba + reserved + num_fats*fat_size
} fat32_t;

// Sadece kısa (8.3) isimlerle çalışacak basit dirent
typedef struct {
    uint8_t  name11[11];       // "NAME    EXT"
    uint8_t  attr;             // FAT_ATTR_*
    uint32_t first_cluster;    // (hi<<16 | lo)
    uint32_t size;             // bytes
} fat32_dirent_t;

// mount + helpers
int      fat32_mount(blockdev_t* dev, uint32_t part_lba, fat32_t* out);
uint32_t fat32_cluster_to_lba(const fat32_t* fs, uint32_t cluster);

// FAT/cluster helpers (isteğe bağlı ama çok işine yarar)
uint32_t fat32_fat_get(const fat32_t* fs, uint32_t cluster);
int      fat32_is_eoc(uint32_t v);
int      fat32_read_cluster(const fat32_t* fs, uint32_t cluster, void* out, uint32_t out_bytes);

// 8.3 yardımcıları
int      fat32_name_to_83(const char* in, char out11[11]);
int      fat32_find_root83(const fat32_t* fs, const char* name83_in, fat32_dirent_t* out_ent);

// basit root işlemleri
int      fat32_list_root(const fat32_t* fs,
                         int (*cb)(const char* name, uint32_t size, uint8_t attr, void* u),
                         void* u);

int      fat32_read_file_root83(const fat32_t* fs, const char* name,
                                uint8_t* out, uint32_t cap, uint32_t* out_size);

#ifdef __cplusplus
}
#endif
