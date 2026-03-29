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

typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  ntres;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t last_access_date;
    uint16_t first_cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} fat_dirent_t;
#pragma pack(pop)

#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_LFN       0x0F
#define FAT_ATTR_VOLUME_ID 0x08

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

static void fat_build_short_name(const fat_dirent_t* de, char* out, int out_sz)
{
    int p = 0;
    if (!de || !out || out_sz <= 0) return;

    for (int i = 0; i < 8 && p < out_sz - 1; i++) {
        char c = (char)de->name[i];
        if (c == ' ') break;
        out[p++] = c;
    }

    int has_ext = 0;
    for (int i = 8; i < 11; i++) {
        if (de->name[i] != ' ') {
            has_ext = 1;
            break;
        }
    }

    if (has_ext && p < out_sz - 1) {
        out[p++] = '.';
        for (int i = 8; i < 11 && p < out_sz - 1; i++) {
            char c = (char)de->name[i];
            if (c == ' ') break;
            out[p++] = c;
        }
    }

    out[p] = '\0';
}

static int fat_name_equals_83(const fat_dirent_t* de, const char* wanted)
{
    char tmp[20];
    fat_build_short_name(de, tmp, sizeof(tmp));
    return strcmp(tmp, wanted) == 0;
}

static int fat_make_83_name(const char* in, uint8_t out_name[11])
{
    if (!in || !in[0] || !out_name) return 0;

    for (int i = 0; i < 11; i++) out_name[i] = ' ';

    int base_len = 0;
    int ext_len = 0;
    const char* dot = 0;

    for (const char* p = in; *p; p++) {
        if (*p == '.') {
            if (dot) return 0; /* birden fazla nokta yok */
            dot = p;
        }
    }

    if (dot) {
        base_len = (int)(dot - in);
        ext_len = (int)strlen(dot + 1);
    } else {
        base_len = (int)strlen(in);
        ext_len = 0;
    }

    if (base_len < 1 || base_len > 8) return 0;
    if (ext_len > 3) return 0;

    for (int i = 0; i < base_len; i++) {
        char c = in[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        if (c == '/' || c == '\\' || c == ' ') return 0;
        out_name[i] = (uint8_t)c;
    }

    if (dot) {
        for (int i = 0; i < ext_len; i++) {
            char c = dot[1 + i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            if (c == '/' || c == '\\' || c == ' ') return 0;
            out_name[8 + i] = (uint8_t)c;
        }
    }

    return 1;
}

static uint32_t fat_cluster_to_lba(const fat_info_t* info, uint32_t cluster)
{
    if (!info || cluster < 2) return 0;
    return info->first_data_sector + ((cluster - 2) * info->sectors_per_cluster);
}

static uint32_t fat16_next_cluster(const fat_info_t* info, uint32_t cluster)
{
    if (!info) return 0;
    if (info->type != FAT_TYPE_16) return 0;
    if (cluster < 2) return 0;

    uint32_t fat_start = info->reserved_sectors;
    uint32_t fat_offset = cluster * 2; // FAT16: 2 byte per entry
    uint32_t fat_sector = fat_start + (fat_offset / info->bytes_per_sector);
    uint32_t ent_offset = fat_offset % info->bytes_per_sector;

    uint8_t sec[512];
    if (!block_read(fat_sector, 1, sec)) {
        return 0;
    }

    uint16_t next = *(uint16_t*)(sec + ent_offset);

    // FAT16 end-of-chain
    if (next >= 0xFFF8) return 0xFFFFFFFF;
    if (next == 0xFFF7) return 0; // bad cluster
    if (next == 0x0000 || next == 0x0001) return 0;

    return (uint32_t)next;
}

static int fat_find_root_entry(const fat_info_t* info, const char* name83, fat_dirent_t* out_de)
{
    if (!info || !name83 || !out_de) return 0;
    if (info->type != FAT_TYPE_12 && info->type != FAT_TYPE_16) return 0;

    uint32_t root_dir_start =
        info->reserved_sectors + (info->fat_count * info->sectors_per_fat);

    uint8_t sec[512];

    for (uint32_t s = 0; s < info->root_dir_sectors; s++) {
        if (!block_read(root_dir_start + s, 1, sec)) {
            return 0;
        }

        for (int off = 0; off < 512; off += 32) {
            fat_dirent_t* de = (fat_dirent_t*)(sec + off);

            if (de->name[0] == 0x00) {
                return 0;
            }
            if (de->name[0] == 0xE5) {
                continue;
            }
            if (de->attr == FAT_ATTR_LFN) {
                continue;
            }
            if (de->attr & FAT_ATTR_VOLUME_ID) {
                continue;
            }

            if (fat_name_equals_83(de, name83)) {
                memcpy(out_de, de, sizeof(*out_de));
                return 1;
            }
        }
    }

    return 0;
}

static int fat_find_in_dir_cluster(const fat_info_t* info, uint32_t dir_cluster, const char* name83, fat_dirent_t* out_de)
{
    if (!info || !name83 || !out_de) return 0;
    if (info->type != FAT_TYPE_16) return 0;
    if (dir_cluster < 2) return 0;

    uint8_t sec[512];
    uint32_t cluster = dir_cluster;

    while (cluster >= 2) {
        uint32_t lba = fat_cluster_to_lba(info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info->sectors_per_cluster; s++) {
            if (!block_read(lba + s, 1, sec)) return 0;

            for (int off = 0; off < 512; off += 32) {
                fat_dirent_t* de = (fat_dirent_t*)(sec + off);

                if (de->name[0] == 0x00) {
                    return 0;
                }
                if (de->name[0] == 0xE5) {
                    continue;
                }
                if (de->attr == FAT_ATTR_LFN) {
                    continue;
                }
                if (de->attr & FAT_ATTR_VOLUME_ID) {
                    continue;
                }

                if (fat_name_equals_83(de, name83)) {
                    memcpy(out_de, de, sizeof(*out_de));
                    return 1;
                }
            }
        }

        uint32_t next = fat16_next_cluster(info, cluster);
        if (next == 0xFFFFFFFF) break;
        if (next < 2) return 0;
        cluster = next;
    }

    return 0;
}

static int fat_list_dir_cluster(const fat_info_t* info, uint32_t dir_cluster)
{
    if (!info) return 0;
    if (info->type != FAT_TYPE_16) return 0;
    if (dir_cluster < 2) return 0;

    uint8_t sec[512];
    uint32_t cluster = dir_cluster;

    while (cluster >= 2) {
        uint32_t lba = fat_cluster_to_lba(info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info->sectors_per_cluster; s++) {
            if (!block_read(lba + s, 1, sec)) return 0;

            for (int off = 0; off < 512; off += 32) {
                fat_dirent_t* de = (fat_dirent_t*)(sec + off);

                if (de->name[0] == 0x00) {
                    return 1;
                }
                if (de->name[0] == 0xE5) {
                    continue;
                }
                if (de->attr == FAT_ATTR_LFN) {
                    continue;
                }
                if (de->attr & FAT_ATTR_VOLUME_ID) {
                    continue;
                }

                char namebuf[20];
                fat_build_short_name(de, namebuf, sizeof(namebuf));

                if (strcmp(namebuf, ".") == 0 || strcmp(namebuf, "..") == 0) {
                    continue;
                }

                if (de->attr & FAT_ATTR_DIRECTORY) {
                    printk("[DIR]  %s\n", namebuf);
                } else {
                    printk("%d byte  %s\n", de->file_size, namebuf);
                }
            }
        }

        uint32_t next = fat16_next_cluster(info, cluster);
        if (next == 0xFFFFFFFF) break;
        if (next < 2) return 0;
        cluster = next;
    }

    return 1;
}

static int fat_resolve_path(const fat_info_t* info, const char* path, fat_dirent_t* out_de, int* out_is_root)
{
    if (!info || !path) return 0;

    if (strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
        if (out_is_root) *out_is_root = 1;
        return 1;
    }

    if (out_is_root) *out_is_root = 0;

    char tmp[256];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;

    char* p = tmp;
    while (*p == '/') p++;

    uint32_t current_cluster = 0;
    int in_root = 1;

    char* token = p;

    while (token && *token) {
        while (*token == '/') token++;
        if (!*token) break;

        char* slash = token;
        while (*slash && *slash != '/') slash++;

        char saved = *slash;
        *slash = 0;

        fat_dirent_t de;
        int found = 0;

        if (in_root) {
            found = fat_find_root_entry(info, token, &de);
        } else {
            found = fat_find_in_dir_cluster(info, current_cluster, token, &de);
        }

        if (!found) return 0;

        current_cluster =
            ((uint32_t)de.first_cluster_hi << 16) |
            (uint32_t)de.first_cluster_lo;

        if (saved == 0) {
            if (out_de) memcpy(out_de, &de, sizeof(de));
            return 1;
        }

        if (!(de.attr & FAT_ATTR_DIRECTORY)) {
            return 0;
        }

        in_root = 0;
        token = slash + 1;
    }

    return 0;
}

bool fat_probe_from_sector0(const uint8_t* sector, fat_info_t* out)
{
    if (!sector || !out) return false;

    memset(out, 0, sizeof(*out));

    const fat_bpb_t* bpb = (const fat_bpb_t*)sector;

    uint16_t sig = *(const uint16_t*)(sector + 510);
    if (sig != 0xAA55) return false;

    if (bpb->bytes_per_sector == 0) return false;
    if (bpb->sectors_per_cluster == 0) return false;
    if (bpb->num_fats == 0) return false;

    uint32_t total_sectors    = fat_total_sectors(bpb);
    uint32_t sectors_per_fat  = fat_sectors_per_fat(bpb);
    uint32_t root_dir_sectors =
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

void fat_test_list_root(void)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!block_has_root()) {
        printk("[FAT] no root block device\n");
        return;
    }

    if (!block_read(0, 1, boot)) {
        printk("[FAT] failed to read sector 0\n");
        return;
    }

    if (!fat_probe_from_sector0(boot, &info)) {
        printk("[FAT] cannot list root: invalid FAT boot sector\n");
        return;
    }

    if (info.type != FAT_TYPE_16 && info.type != FAT_TYPE_12) {
        printk("[FAT] root listing test currently supports FAT12/16 only\n");
        return;
    }

    uint32_t root_dir_start =
        info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

    printk("[FAT] ROOT DIR start=%u sectors=%u\n",
           root_dir_start, info.root_dir_sectors);

    uint8_t sec[512];
    int shown = 0;

    for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
        if (!block_read(root_dir_start + s, 1, sec)) {
            printk("[FAT] failed to read root dir sector %u\n", root_dir_start + s);
            return;
        }

        for (int off = 0; off < 512; off += 32) {
            fat_dirent_t* de = (fat_dirent_t*)(sec + off);

            if (de->name[0] == 0x00) {
                printk("[FAT] root listing done (%d entries)\n", shown);
                return;
            }
            if (de->name[0] == 0xE5) {
                continue;
            }
            if (de->attr == FAT_ATTR_LFN) {
                continue;
            }
            if (de->attr & FAT_ATTR_VOLUME_ID) {
                continue;
            }

            char namebuf[20];
            fat_build_short_name(de, namebuf, sizeof(namebuf));

            uint32_t first_cluster =
                ((uint32_t)de->first_cluster_hi << 16) |
                (uint32_t)de->first_cluster_lo;

            if (de->attr & FAT_ATTR_DIRECTORY) {
                printk("[FAT] DIR  %s cluster=%u\n", namebuf, first_cluster);
            } else {
                printk("[FAT] FILE %s size=%u cluster=%u\n",
                       namebuf, de->file_size, first_cluster);
            }

            shown++;
        }
    }

    printk("[FAT] root listing done (%d entries)\n", shown);
}

static int fat_read_file_from_root(const char* name83, char* out, uint32_t out_cap, uint32_t* out_size)
{
    uint8_t boot[512];
    fat_info_t info;
    fat_dirent_t de;

    if (!out || out_cap == 0) return 0;
    out[0] = '\0';

    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;

    if (info.type != FAT_TYPE_12 && info.type != FAT_TYPE_16) return 0;

    if (!fat_find_root_entry(&info, name83, &de)) return 0;
    if (de.attr & FAT_ATTR_DIRECTORY) return 0;

    uint32_t file_size =
        de.file_size;

    uint32_t cluster =
        ((uint32_t)de.first_cluster_hi << 16) |
        (uint32_t)de.first_cluster_lo;

    if (cluster < 2) return 0;

    uint32_t copied = 0;
    uint8_t sec[512];

    while (cluster >= 2 && copied < file_size) {
        uint32_t lba = fat_cluster_to_lba(&info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info.sectors_per_cluster && copied < file_size; s++) {
            if (!block_read(lba + s, 1, sec)) return 0;

            uint32_t remain = file_size - copied;
            uint32_t take = (remain > 512) ? 512 : remain;

            if (copied + take > out_cap - 1) {
                take = (out_cap - 1) - copied;
            }

            memcpy(out + copied, sec, take);
            copied += take;

            if (copied >= out_cap - 1) {
                out[copied] = '\0';
                if (out_size) *out_size = copied;
                return 1;
            }
        }

        if (copied >= file_size) break;

        if (info.type == FAT_TYPE_16) {
            uint32_t next = fat16_next_cluster(&info, cluster);
            if (next == 0xFFFFFFFF) break;
            if (next < 2) return 0;
            cluster = next;
        } else {
            // FAT12 chain henüz yok
            return 0;
        }
    }

    out[copied] = '\0';
    if (out_size) *out_size = copied;
    return 1;
}

void fat_test_read_hello(void)
{
    char textbuf[256];
    uint32_t sz = 0;

    memset(textbuf, 0, sizeof(textbuf));

    if (!fat_read_file_from_root("HELLO.TXT", textbuf, sizeof(textbuf), &sz)) {
        printk("[FAT] failed to read HELLO.TXT\n");
        return;
    }

    printk("[FAT] HELLO.TXT content (%u bytes): \"%s\"\n", sz, textbuf);
}

void fat_test_read_bigfile(void)
{
    static char bigbuf[8192];
    uint32_t sz = 0;

    memset(bigbuf, 0, sizeof(bigbuf));

    if (!fat_read_file_from_root("BIGFILE.TXT", bigbuf, sizeof(bigbuf), &sz)) {
        printk("[FAT] BIGFILE.TXT not found or read failed\n");
        return;
    }

    printk("[FAT] BIGFILE.TXT read ok (%u bytes)\n", sz);
    printk("[FAT] BIGFILE preview: \"%.120s\"\n", bigbuf);
}

int fat_list_root_cmd(void)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!block_has_root()) {
        printk("[FAT] no root block device\n");
        return 0;
    }

    if (!block_read(0, 1, boot)) {
        printk("[FAT] failed to read sector 0\n");
        return 0;
    }

    if (!fat_probe_from_sector0(boot, &info)) {
        printk("[FAT] invalid FAT boot sector\n");
        return 0;
    }

    if (info.type != FAT_TYPE_16 && info.type != FAT_TYPE_12) {
        printk("[FAT] list currently supports FAT12/16 only\n");
        return 0;
    }

    uint32_t root_dir_start =
        info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

    uint8_t sec[512];

    for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
        if (!block_read(root_dir_start + s, 1, sec)) {
            printk("[FAT] failed to read root dir sector %u\n", root_dir_start + s);
            return 0;
        }

        for (int off = 0; off < 512; off += 32) {
            fat_dirent_t* de = (fat_dirent_t*)(sec + off);

            if (de->name[0] == 0x00) {
                return 1;
            }
            if (de->name[0] == 0xE5) {
                continue;
            }
            if (de->attr == FAT_ATTR_LFN) {
                continue;
            }
            if (de->attr & FAT_ATTR_VOLUME_ID) {
                continue;
            }

            char namebuf[20];
            fat_build_short_name(de, namebuf, sizeof(namebuf));

            if (de->attr & FAT_ATTR_DIRECTORY) {
                printk("[DIR]  %s\n", namebuf);
            } else {
                printk("%d byte  %s\n", de->file_size, namebuf);
            }
        }
    }

    return 1;
}

int fat_read_root_file(const char* name83, uint8_t* out, uint32_t out_cap, uint32_t* out_size)
{
    return fat_read_file_from_root(name83, (char*)out, out_cap, out_size);
}

static uint32_t fat16_find_free_cluster(const fat_info_t* info)
{
    if (!info) return 0;
    if (info->type != FAT_TYPE_16) return 0;

    uint8_t sec[512];
    uint32_t max_cluster = info->cluster_count + 1; /* yaklaşık üst sınır */

    for (uint32_t cluster = 2; cluster <= max_cluster; cluster++) {
        uint32_t fat_start  = info->reserved_sectors;
        uint32_t fat_offset = cluster * 2;
        uint32_t fat_sector = fat_start + (fat_offset / info->bytes_per_sector);
        uint32_t ent_offset = fat_offset % info->bytes_per_sector;

        if (!block_read(fat_sector, 1, sec)) return 0;

        uint16_t val = *(uint16_t*)(sec + ent_offset);
        if (val == 0x0000) {
            return cluster;
        }
    }

    return 0;
}

static int fat16_write_fat_entry(const fat_info_t* info, uint32_t cluster, uint16_t value)
{
    if (!info) return 0;
    if (info->type != FAT_TYPE_16) return 0;
    if (cluster < 2) return 0;

    uint32_t fat_start  = info->reserved_sectors;
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start + (fat_offset / info->bytes_per_sector);
    uint32_t ent_offset = fat_offset % info->bytes_per_sector;

    /* her FAT kopyasına yaz */
    for (uint32_t fat_i = 0; fat_i < info->fat_count; fat_i++) {
        uint32_t lba = fat_sector + (fat_i * info->sectors_per_fat);

        uint8_t sec[512];
        if (!block_read(lba, 1, sec)) return 0;

        *(uint16_t*)(sec + ent_offset) = value;

        if (!block_write(lba, 1, sec)) return 0;
    }

    return 1;
}

static int fat_zero_cluster(const fat_info_t* info, uint32_t cluster)
{
    if (!info) return 0;
    if (cluster < 2) return 0;

    uint32_t lba = fat_cluster_to_lba(info, cluster);
    if (lba == 0) return 0;

    uint8_t zero[512];
    memset(zero, 0, sizeof(zero));

    for (uint32_t s = 0; s < info->sectors_per_cluster; s++) {
        if (!block_write(lba + s, 1, zero)) return 0;
    }

    return 1;
}

int fat_create_root_file(const char* name83)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!name83 || !name83[0]) return 0;
    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_16 && info.type != FAT_TYPE_12) return 0;

    /* aynı isim var mı? */
    fat_dirent_t existing;
    if (fat_find_root_entry(&info, name83, &existing)) {
        return 0; /* overwrite yok */
    }

    uint8_t dos_name[11];
    if (!fat_make_83_name(name83, dos_name)) {
        return 0;
    }

    uint32_t root_dir_start =
        info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

    uint8_t sec[512];

    for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
        uint32_t lba = root_dir_start + s;

        if (!block_read(lba, 1, sec)) {
            return 0;
        }

        for (int off = 0; off < 512; off += 32) {
            fat_dirent_t* de = (fat_dirent_t*)(sec + off);

            /* boş ya da silinmiş entry kullanılabilir */
            if (de->name[0] == 0x00 || de->name[0] == 0xE5) {
                memset(de, 0, sizeof(*de));
                memcpy(de->name, dos_name, 11);
                de->attr = 0x00; /* normal file */
                de->first_cluster_lo = 0;
                de->first_cluster_hi = 0;
                de->file_size = 0;

                if (!block_write(lba, 1, sec)) {
                    return 0;
                }

                return 1;
            }
        }
    }

    return 0; /* root dolu */
}

int fat_create_root_dir(const char* name83)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!name83 || !name83[0]) return 0;
    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_16) return 0; /* şimdilik sadece FAT16 */

    /* aynı isim var mı? */
    fat_dirent_t existing;
    if (fat_find_root_entry(&info, name83, &existing)) {
        return 0;
    }

    uint8_t dos_name[11];
    if (!fat_make_83_name(name83, dos_name)) {
        return 0;
    }

    /* yeni cluster ayır */
    uint32_t new_cluster = fat16_find_free_cluster(&info);
    if (new_cluster < 2) return 0;

    /* FAT chain: tek cluster directory => EOC */
    if (!fat16_write_fat_entry(&info, new_cluster, 0xFFFF)) {
        return 0;
    }

    /* cluster sıfırla */
    if (!fat_zero_cluster(&info, new_cluster)) {
        return 0;
    }

    /* "." ve ".." entry yaz */
    uint32_t dir_lba = fat_cluster_to_lba(&info, new_cluster);
    if (dir_lba == 0) return 0;

    uint8_t sec[512];
    memset(sec, 0, sizeof(sec));

    fat_dirent_t* dot = (fat_dirent_t*)(sec + 0);
    memset(dot, 0, sizeof(*dot));
    memcpy(dot->name, ".          ", 11);
    dot->attr = FAT_ATTR_DIRECTORY;
    dot->first_cluster_lo = (uint16_t)new_cluster;
    dot->first_cluster_hi = 0;
    dot->file_size = 0;

    fat_dirent_t* dotdot = (fat_dirent_t*)(sec + 32);
    memset(dotdot, 0, sizeof(*dotdot));
    memcpy(dotdot->name, "..         ", 11);
    dotdot->attr = FAT_ATTR_DIRECTORY;
    /* root parent => cluster 0 */
    dotdot->first_cluster_lo = 0;
    dotdot->first_cluster_hi = 0;
    dotdot->file_size = 0;

    if (!block_write(dir_lba, 1, sec)) {
        return 0;
    }

    /* root directory entry ekle */
    uint32_t root_dir_start =
        info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

    for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
        uint32_t lba = root_dir_start + s;

        if (!block_read(lba, 1, sec)) {
            return 0;
        }

        for (int off = 0; off < 512; off += 32) {
            fat_dirent_t* de = (fat_dirent_t*)(sec + off);

            if (de->name[0] == 0x00 || de->name[0] == 0xE5) {
                memset(de, 0, sizeof(*de));
                memcpy(de->name, dos_name, 11);
                de->attr = FAT_ATTR_DIRECTORY;
                de->first_cluster_lo = (uint16_t)new_cluster;
                de->first_cluster_hi = 0;
                de->file_size = 0;

                if (!block_write(lba, 1, sec)) {
                    return 0;
                }

                return 1;
            }
        }
    }

    return 0;
}

int fat_list_path(const char* path)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_16 && info.type != FAT_TYPE_12) return 0;

    if (strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
        return fat_list_root_cmd();
    }

    fat_dirent_t de;
    int is_root = 0;

    if (!fat_resolve_path(&info, path, &de, &is_root)) {
        return 0;
    }

    if (is_root) {
        return fat_list_root_cmd();
    }

    if (!(de.attr & FAT_ATTR_DIRECTORY)) {
        return 0;
    }

    uint32_t cluster =
        ((uint32_t)de.first_cluster_hi << 16) |
        (uint32_t)de.first_cluster_lo;

    return fat_list_dir_cluster(&info, cluster);
}

int fat_read_file_path(const char* path, uint8_t* out, uint32_t out_cap, uint32_t* out_size)
{
    uint8_t boot[512];
    fat_info_t info;
    fat_dirent_t de;

    if (!out || out_cap == 0) return 0;
    out[0] = 0;

    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_12 && info.type != FAT_TYPE_16) return 0;

    int is_root = 0;
    if (!fat_resolve_path(&info, path, &de, &is_root)) {
        return 0;
    }

    if (is_root) return 0;
    if (de.attr & FAT_ATTR_DIRECTORY) return 0;

    uint32_t file_size = de.file_size;
    uint32_t cluster =
        ((uint32_t)de.first_cluster_hi << 16) |
        (uint32_t)de.first_cluster_lo;

    /* 0-byte FAT dosyasi: cluster 0 olabilir, bu normal */
    if (file_size == 0) {
        out[0] = 0;
        if (out_size) *out_size = 0;
        return 1;
    }

    if (cluster < 2) return 0;

    uint32_t copied = 0;
    uint8_t sec[512];

    while (cluster >= 2 && copied < file_size) {
        uint32_t lba = fat_cluster_to_lba(&info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info.sectors_per_cluster && copied < file_size; s++) {
            if (!block_read(lba + s, 1, sec)) return 0;

            uint32_t remain = file_size - copied;
            uint32_t take = (remain > 512) ? 512 : remain;

            if (copied + take > out_cap - 1) {
                take = (out_cap - 1) - copied;
            }

            memcpy(out + copied, sec, take);
            copied += take;

            if (copied >= out_cap - 1) {
                out[copied] = 0;
                if (out_size) *out_size = copied;
                return 1;
            }
        }

        if (copied >= file_size) break;

        if (info.type == FAT_TYPE_16) {
            uint32_t next = fat16_next_cluster(&info, cluster);
            if (next == 0xFFFFFFFF) break;
            if (next < 2) return 0;
            cluster = next;
        } else {
            return 0;
        }
    }

    out[copied] = 0;
    if (out_size) *out_size = copied;
    return 1;
}

int fat_create_file_path(const char* path)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!path) return 0;
    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;

    /* root mu? */
    const char* last = strrchr(path, '/');

    if (!last) {
        return fat_create_root_file(path);
    }

    char parent[256];
    int len = last - path;

    if (len <= 0) {
        return fat_create_root_file(last + 1);
    }

    strncpy(parent, path, len);
    parent[len] = 0;

    const char* filename = last + 1;

    fat_dirent_t de;
    int is_root = 0;

    if (!fat_resolve_path(&info, parent, &de, &is_root)) {
        return 0;
    }

    if (is_root) {
        return fat_create_root_file(filename);
    }

    if (!(de.attr & FAT_ATTR_DIRECTORY)) return 0;

    uint32_t cluster =
        ((uint32_t)de.first_cluster_hi << 16) |
        (uint32_t)de.first_cluster_lo;

    return fat_create_file_in_dir_cluster(&info, cluster, filename);
}

int fat_path_exists(const char* path)
{
    uint8_t boot[512];
    fat_info_t info;

    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_16 && info.type != FAT_TYPE_12) return 0;

    if (!path || strcmp(path, "") == 0 || strcmp(path, "/") == 0) {
        return 1;
    }

    fat_dirent_t de;
    int is_root = 0;

    if (!fat_resolve_path(&info, path, &de, &is_root)) {
        return 0;
    }

    if (is_root) return 1;

    if (de.attr & FAT_ATTR_DIRECTORY) {
        return 1;
    }

    return 0;
}

int fat_create_file_in_dir_cluster(
    const fat_info_t* info,
    uint32_t dir_cluster,
    const char* name83)
{
    if (!info || !name83) return 0;
    if (info->type != FAT_TYPE_16) return 0;

    uint8_t dos_name[11];
    if (!fat_make_83_name(name83, dos_name)) return 0;

    uint32_t cluster = dir_cluster;
    uint8_t sec[512];

    while (cluster >= 2) {
        uint32_t lba = fat_cluster_to_lba(info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info->sectors_per_cluster; s++) {
            uint32_t cur_lba = lba + s;

            if (!block_read(cur_lba, 1, sec)) return 0;

            for (int off = 0; off < 512; off += 32) {
                fat_dirent_t* de = (fat_dirent_t*)(sec + off);

                if (de->name[0] == 0x00 || de->name[0] == 0xE5) {
                    memset(de, 0, sizeof(*de));
                    memcpy(de->name, dos_name, 11);
                    de->attr = 0x00;
                    de->first_cluster_lo = 0;
                    de->first_cluster_hi = 0;
                    de->file_size = 0;

                    if (!block_write(cur_lba, 1, sec)) return 0;
                    return 1;
                }
            }
        }

        uint32_t next = fat16_next_cluster(info, cluster);
        if (next == 0xFFFFFFFF) break;
        if (next < 2) return 0;
        cluster = next;
    }

    return 0;
}

int fat_write_file_path(const char* path, const uint8_t* data, uint32_t size)
{
    if (!path || !data) return 0;

    uint8_t boot[512];
    fat_info_t info;
    fat_dirent_t de;
    int is_root = 0;

    if (!block_has_root()) return 0;
    if (!block_read(0, 1, boot)) return 0;
    if (!fat_probe_from_sector0(boot, &info)) return 0;
    if (info.type != FAT_TYPE_16) return 0;

    if (!fat_resolve_path(&info, path, &de, &is_root)) return 0;
    if (is_root) return 0;
    if (de.attr & FAT_ATTR_DIRECTORY) return 0;

    uint32_t cluster_cap = info.sectors_per_cluster * info.bytes_per_sector;
    if (size > cluster_cap) return 0; /* ilk sürüm: tek cluster */

    uint32_t new_cluster = fat16_find_free_cluster(&info);
    if (new_cluster < 2) return 0;

    if (!fat16_write_fat_entry(&info, new_cluster, 0xFFFF)) return 0;
    if (!fat_zero_cluster(&info, new_cluster)) return 0;

    uint32_t lba = fat_cluster_to_lba(&info, new_cluster);
    if (lba == 0) return 0;

    uint8_t sec[512];
    uint32_t written = 0;

    for (uint32_t s = 0; s < info.sectors_per_cluster; s++) {
        memset(sec, 0, sizeof(sec));

        if (written < size) {
            uint32_t remain = size - written;
            uint32_t take = (remain > 512) ? 512 : remain;
            memcpy(sec, data + written, take);
            written += take;
        }

        if (!block_write(lba + s, 1, sec)) return 0;
    }

    /* entry güncelle */
    {
        const char* last = strrchr(path, '/');
        const char* filename = last ? last + 1 : path;

        uint8_t dirsec[512];

        if (!last || last == path) {
            /* root entry güncelle */
            uint32_t root_dir_start =
                info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

            for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
                uint32_t dlba = root_dir_start + s;
                if (!block_read(dlba, 1, dirsec)) return 0;

                for (int off = 0; off < 512; off += 32) {
                    fat_dirent_t* e = (fat_dirent_t*)(dirsec + off);

                    if (e->name[0] == 0x00) break;
                    if (e->name[0] == 0xE5) continue;
                    if (e->attr == FAT_ATTR_LFN) continue;
                    if (e->attr & FAT_ATTR_VOLUME_ID) continue;

                    if (fat_name_equals_83(e, filename)) {
                        e->first_cluster_lo = (uint16_t)new_cluster;
                        e->first_cluster_hi = 0;
                        e->file_size = size;
                        if (!block_write(dlba, 1, dirsec)) return 0;
                        return 1;
                    }
                }
            }

            return 0;
        } else {
            /* parent dir entry güncelle */
            char parent[256];
            int len = (int)(last - path);
            if (len <= 0 || len >= (int)sizeof(parent)) return 0;

            strncpy(parent, path, len);
            parent[len] = 0;

            fat_dirent_t parent_de;
            int parent_is_root = 0;

            if (!fat_resolve_path(&info, parent, &parent_de, &parent_is_root)) return 0;

            if (parent_is_root) {
                uint32_t root_dir_start =
                    info.reserved_sectors + (info.fat_count * info.sectors_per_fat);

                for (uint32_t s = 0; s < info.root_dir_sectors; s++) {
                    uint32_t dlba = root_dir_start + s;
                    if (!block_read(dlba, 1, dirsec)) return 0;

                    for (int off = 0; off < 512; off += 32) {
                        fat_dirent_t* e = (fat_dirent_t*)(dirsec + off);

                        if (e->name[0] == 0x00) break;
                        if (e->name[0] == 0xE5) continue;
                        if (e->attr == FAT_ATTR_LFN) continue;
                        if (e->attr & FAT_ATTR_VOLUME_ID) continue;

                        if (fat_name_equals_83(e, filename)) {
                            e->first_cluster_lo = (uint16_t)new_cluster;
                            e->first_cluster_hi = 0;
                            e->file_size = size;
                            if (!block_write(dlba, 1, dirsec)) return 0;
                            return 1;
                        }
                    }
                }

                return 0;
            }

            uint32_t parent_cluster =
                ((uint32_t)parent_de.first_cluster_hi << 16) |
                (uint32_t)parent_de.first_cluster_lo;

            uint32_t cluster = parent_cluster;

            while (cluster >= 2) {
                uint32_t plba = fat_cluster_to_lba(&info, cluster);
                if (plba == 0) return 0;

                for (uint32_t s = 0; s < info.sectors_per_cluster; s++) {
                    uint32_t dlba = plba + s;
                    if (!block_read(dlba, 1, dirsec)) return 0;

                    for (int off = 0; off < 512; off += 32) {
                        fat_dirent_t* e = (fat_dirent_t*)(dirsec + off);

                        if (e->name[0] == 0x00) break;
                        if (e->name[0] == 0xE5) continue;
                        if (e->attr == FAT_ATTR_LFN) continue;
                        if (e->attr & FAT_ATTR_VOLUME_ID) continue;

                        if (fat_name_equals_83(e, filename)) {
                            e->first_cluster_lo = (uint16_t)new_cluster;
                            e->first_cluster_hi = 0;
                            e->file_size = size;
                            if (!block_write(dlba, 1, dirsec)) return 0;
                            return 1;
                        }
                    }
                }

                uint32_t next = fat16_next_cluster(&info, cluster);
                if (next == 0xFFFFFFFF) break;
                if (next < 2) return 0;
                cluster = next;
            }

            return 0;
        }
    }
}

int fat_append_file_path(const char* path, const uint8_t* data, uint32_t size)
{
    if (!path || !data) return 0;

    uint8_t oldbuf[8192];
    uint32_t oldsz = 0;

    /* dosya varsa oku, yoksa boş kabul et */
    if (!fat_read_file_path(path, oldbuf, sizeof(oldbuf) - 1, &oldsz)) {
        oldsz = 0;
    }

    if (oldsz + size >= sizeof(oldbuf)) return 0;

    memcpy(oldbuf + oldsz, data, size);
    oldsz += size;
    oldbuf[oldsz] = 0;

    return fat_write_file_path(path, oldbuf, oldsz);
}