#pragma once

#include <stdint.h>
#include <kernel/block/blockdev.h>

typedef enum {
    FAT_TYPE_NONE = 0,
    FAT_TYPE_12,
    FAT_TYPE_16,
    FAT_TYPE_32
} fat_type_t;

typedef struct {
    fat_type_t type;
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint32_t sectors_per_fat;
    uint32_t root_dir_sectors;
    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t cluster_count;
    uint32_t root_cluster;
} fat_info_t;

typedef int (*fat_iter_cb)(const char* name, uint32_t size, int is_dir, void* u);

int fat_probe_sector0(const uint8_t* sector, fat_info_t* out);
int fat_probe_dev(blockdev_t* dev, fat_info_t* out);
int fat_iter_root(blockdev_t* dev, fat_iter_cb cb, void* u);
int fat_iter_path(blockdev_t* dev, const char* path, fat_iter_cb cb, void* u);
int fat_stat_path(blockdev_t* dev, const char* path, uint32_t* out_size, int* out_is_dir);
const char* fat_type_name(fat_type_t type);