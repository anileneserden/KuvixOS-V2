#include <kernel/fs/fat.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>
#include <lib/string.h>

#pragma pack(push, 1)
typedef struct {
    uint8_t  jmp_boot[3];
    uint8_t  oem_name[8];

    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    union {
        struct {
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_signature;
            uint32_t volume_id;
            uint8_t  volume_label[11];
            uint8_t  fs_type[8];
            uint8_t  boot_code[448];
            uint16_t boot_sector_signature;
        } fat16;

        struct {
            uint32_t fat_size_32;
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;
            uint16_t fs_info;
            uint16_t backup_boot_sector;
            uint8_t  reserved[12];
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_signature;
            uint32_t volume_id;
            uint8_t  volume_label[11];
            uint8_t  fs_type[8];
            uint8_t  boot_code[420];
            uint16_t boot_sector_signature;
        } fat32;
    };
} fat_bpb_t;
#pragma pack(pop)

static uint32_t fat_total_sectors(const fat_bpb_t* bpb)
{
    if (bpb->total_sectors_16 != 0) return bpb->total_sectors_16;
    return bpb->total_sectors_32;
}

static uint32_t fat_sectors_per_fat(const fat_bpb_t* bpb)
{
    if (bpb->fat_size_16 != 0) return bpb->fat_size_16;
    return bpb->fat32.fat_size_32;
}

static const char* fat_type_name(fat_type_t t)
{
    switch (t) {
        case FAT_TYPE_12: return "FAT12";
        case FAT_TYPE_16: return "FAT16";
        case FAT_TYPE_32: return "FAT32";
        default:          return "NONE";
    }
}

bool fat_probe_from_sector0(const uint8_t* sector, fat_info_t* out)
{
    if (!sector || !out) return false;

    memset(out, 0, sizeof(*out));

    const fat_bpb_t* bpb = (const fat_bpb_t*)sector;

    /* 0x55AA imzası */
    uint16_t sig = *(const uint16_t*)(sector + 510);
    if (sig != 0xAA55) return false;

    if (bpb->bytes_per_sector == 0) return false;
    if (bpb->sectors_per_cluster == 0) return false;
    if (bpb->num_fats == 0) return false;

    uint32_t total_sectors     = fat_total_sectors(bpb);
    uint32_t sectors_per_fat   = fat_sectors_per_fat(bpb);
    uint32_t root_dir_sectors  =
        ((uint32_t)bpb->root_entry_count * 32U + (bpb->bytes_per_sector - 1U)) /
        bpb->bytes_per_sector;

    uint32_t first_data_sector =
        (uint32_t)bpb->reserved_sector_count +
        ((uint32_t)bpb->num_fats * sectors_per_fat) +
        root_dir_sectors;

    if (total_sectors < first_data_sector) return false;

    uint32_t data_sectors =
        total_sectors - first_data_sector;

    uint32_t cluster_count =
        data_sectors / bpb->sectors_per_cluster;

    fat_type_t type = FAT_TYPE_NONE;
    if (cluster_count < 4085) {
        type = FAT_TYPE_12;
    } else if (cluster_count < 65525) {
        type = FAT_TYPE_16;
    } else {
        type = FAT_TYPE_32;
    }

    out->type                = type;
    out->bytes_per_sector    = bpb->bytes_per_sector;
    out->sectors_per_cluster = bpb->sectors_per_cluster;
    out->reserved_sectors    = bpb->reserved_sector_count;
    out->fat_count           = bpb->num_fats;
    out->root_entry_count    = bpb->root_entry_count;
    out->total_sectors       = total_sectors;
    out->sectors_per_fat     = sectors_per_fat;
    out->root_dir_sectors    = root_dir_sectors;
    out->first_data_sector   = first_data_sector;
    out->data_sectors        = data_sectors;
    out->cluster_count       = cluster_count;
    out->root_cluster        = (type == FAT_TYPE_32) ? bpb->fat32.root_cluster : 0;

    return true;
}

void fat_debug_dump(const fat_info_t* info)
{
    if (!info) return;

    printk("[FAT] type=%s\n", fat_type_name(info->type));
    printk("[FAT] bytes_per_sector=%u\n", info->bytes_per_sector);
    printk("[FAT] sectors_per_cluster=%u\n", info->sectors_per_cluster);
    printk("[FAT] reserved_sectors=%u\n", info->reserved_sectors);
    printk("[FAT] fat_count=%u\n", info->fat_count);
    printk("[FAT] root_entry_count=%u\n", info->root_entry_count);
    printk("[FAT] total_sectors=%u\n", info->total_sectors);
    printk("[FAT] sectors_per_fat=%u\n", info->sectors_per_fat);
    printk("[FAT] root_dir_sectors=%u\n", info->root_dir_sectors);
    printk("[FAT] first_data_sector=%u\n", info->first_data_sector);
    printk("[FAT] data_sectors=%u\n", info->data_sectors);
    printk("[FAT] cluster_count=%u\n", info->cluster_count);
    if (info->type == FAT_TYPE_32) {
        printk("[FAT] root_cluster=%u\n", info->root_cluster);
    }
}

void fat_test_probe_root(void)
{
    uint8_t sector[512];
    fat_info_t info;

    if (!block_has_root()) {
        printk("[FAT] no root block device\n");
        return;
    }

    if (!block_read(0, 1, sector)) {
        printk("[FAT] failed to read sector 0\n");
        return;
    }

    if (!fat_probe_from_sector0(sector, &info)) {
        printk("[FAT] sector 0 is not a valid FAT boot sector\n");
        return;
    }

    printk("[FAT] FAT boot sector detected on root disk\n");
    fat_debug_dump(&info);
}