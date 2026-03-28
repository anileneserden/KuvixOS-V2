#pragma once
#include <stdint.h>
#include <stdbool.h>

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
    uint32_t root_cluster; // FAT32 için
} fat_info_t;

bool fat_probe_from_sector0(const uint8_t* sector, fat_info_t* out);
void fat_debug_dump(const fat_info_t* info);

void fat_test_probe_root(void);
void fat_test_list_root(void);
void fat_test_read_hello(void);