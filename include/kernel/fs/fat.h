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
    uint32_t root_cluster;
} fat_info_t;

bool fat_probe_from_sector0(const uint8_t* sector, fat_info_t* out);
void fat_debug_dump(const fat_info_t* info);

void fat_test_probe_root(void);
void fat_test_list_root(void);
void fat_test_read_hello(void);
void fat_test_read_bigfile(void);

int fat_list_root_cmd(void);
int fat_read_root_file(const char* name83, uint8_t* out, uint32_t out_cap, uint32_t* out_size);

/* write helpers */
int fat_create_root_file(const char* name83);
int fat_create_root_dir(const char* name83);

/* path helpers */
int fat_list_path(const char* path);
int fat_read_file_path(const char* path, uint8_t* out, uint32_t out_cap, uint32_t* out_size);
int fat_path_exists(const char* path);

int fat_create_file_in_dir_cluster(
    const fat_info_t* info,
    uint32_t dir_cluster,
    const char* name83
);

int fat_create_file_path(const char* path);