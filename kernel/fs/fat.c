#include <kernel/fs/fat.h>
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

#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_LFN       0x0F
#define FAT_ATTR_VOLUME_ID 0x08

typedef struct __attribute__((packed)) {
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

typedef int (*fat_raw_iter_cb)(const fat_dirent_t* de, const char* name, void* u);

typedef struct {
    fat_iter_cb cb;
    void* u;
} fat_public_iter_t;

typedef struct {
    const char* want_name;
    fat_dirent_t out;
    int found;
} fat_find_ctx_t;

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

const char* fat_type_name(fat_type_t type)
{
    switch (type) {
        case FAT_TYPE_12: return "FAT12";
        case FAT_TYPE_16: return "FAT16";
        case FAT_TYPE_32: return "FAT32";
        default: return "NONE";
    }
}

static void fat_build_short_name(const fat_dirent_t* de, char* out, int out_sz)
{
    int p = 0;
    int has_ext = 0;

    if (!de || !out || out_sz <= 0) return;

    for (int i = 0; i < 8 && p < out_sz - 1; i++) {
        char c = (char)de->name[i];
        if (c == ' ') break;
        out[p++] = c;
    }

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

    out[p] = 0;
}

static uint32_t fat_cluster_to_lba(const fat_info_t* info, uint32_t cluster)
{
    if (!info || cluster < 2) return 0;
    return info->first_data_sector + ((cluster - 2u) * info->sectors_per_cluster);
}

static int fat_ascii_eq_icase(const char* a, const char* b)
{
    if (!a || !b) return 0;

    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;

        if (ca >= 'a' && ca <= 'z') ca = (char)(ca - ('a' - 'A'));
        if (cb >= 'a' && cb <= 'z') cb = (char)(cb - ('a' - 'A'));
        if (ca != cb) return 0;
    }

    return (*a == 0 && *b == 0);
}

static uint32_t fat_dirent_first_cluster(const fat_dirent_t* de)
{
    if (!de) return 0;
    return ((uint32_t)de->first_cluster_hi << 16) | (uint32_t)de->first_cluster_lo;
}

static int fat_read_fat_bytes(blockdev_t* dev, const fat_info_t* info, uint32_t fat_offset, uint8_t* out, uint32_t count)
{
    uint8_t sec[512];
    uint32_t cached_sector = 0xFFFFFFFFu;
    uint32_t fat_start = 0;

    if (!dev || !info || !out || count == 0) return 0;
    fat_start = info->reserved_sectors;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t abs_off = fat_offset + i;
        uint32_t fat_sector = fat_start + (abs_off / info->bytes_per_sector);
        uint32_t ent_offset = abs_off % info->bytes_per_sector;

        if (fat_sector != cached_sector) {
            if (!dev->read(dev, fat_sector, 1, sec)) return 0;
            cached_sector = fat_sector;
        }

        out[i] = sec[ent_offset];
    }

    return 1;
}

static uint32_t fat12_next_cluster(blockdev_t* dev, const fat_info_t* info, uint32_t cluster)
{
    uint8_t bytes[2];
    uint32_t fat_offset = 0;
    uint16_t next = 0;

    if (!dev || !info || info->type != FAT_TYPE_12 || cluster < 2) return 0;

    fat_offset = cluster + (cluster / 2u);
    if (!fat_read_fat_bytes(dev, info, fat_offset, bytes, sizeof(bytes))) return 0;

    next = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    if (cluster & 1u) next >>= 4;
    else next &= 0x0FFFu;

    if (next >= 0x0FF8u) return 0xFFFFFFFFu;
    if (next == 0x0FF7u) return 0;
    if (next < 2u) return 0;
    return (uint32_t)next;
}

static uint32_t fat16_next_cluster(blockdev_t* dev, const fat_info_t* info, uint32_t cluster)
{
    uint8_t bytes[2];
    uint32_t fat_offset = 0;
    uint16_t next = 0;

    if (!dev || !info || info->type != FAT_TYPE_16 || cluster < 2) return 0;

    fat_offset = cluster * 2u;
    if (!fat_read_fat_bytes(dev, info, fat_offset, bytes, sizeof(bytes))) return 0;

    next = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    if (next >= 0xFFF8u) return 0xFFFFFFFFu;
    if (next == 0xFFF7u) return 0;
    if (next < 2u) return 0;
    return (uint32_t)next;
}

static uint32_t fat32_next_cluster(blockdev_t* dev, const fat_info_t* info, uint32_t cluster)
{
    uint8_t bytes[4];
    uint32_t fat_offset = 0;
    uint32_t next = 0;

    if (!dev || !info || info->type != FAT_TYPE_32 || cluster < 2) return 0;

    fat_offset = cluster * 4u;
    if (!fat_read_fat_bytes(dev, info, fat_offset, bytes, sizeof(bytes))) return 0;

    next = (uint32_t)bytes[0]
        | ((uint32_t)bytes[1] << 8)
        | ((uint32_t)bytes[2] << 16)
        | ((uint32_t)bytes[3] << 24);
    next &= 0x0FFFFFFFu;

    if (next >= 0x0FFFFFF8u) return 0xFFFFFFFFu;
    if (next == 0x0FFFFFF7u) return 0;
    if (next < 2u) return 0;
    return next;
}

static uint32_t fat_next_cluster(blockdev_t* dev, const fat_info_t* info, uint32_t cluster)
{
    if (!info) return 0;

    switch (info->type) {
        case FAT_TYPE_12: return fat12_next_cluster(dev, info, cluster);
        case FAT_TYPE_16: return fat16_next_cluster(dev, info, cluster);
        case FAT_TYPE_32: return fat32_next_cluster(dev, info, cluster);
        default: return 0;
    }
}

static int fat_public_emit(const fat_dirent_t* de, const char* name, void* u)
{
    fat_public_iter_t* it = (fat_public_iter_t*)u;

    if (!de || !name || !it || !it->cb) return -1;
    return it->cb(name, de->file_size, (de->attr & FAT_ATTR_DIRECTORY) ? 1 : 0, it->u);
}

static int fat_iter_sector_entries(const uint8_t* sec, fat_raw_iter_cb cb, void* u, int* hit_end)
{
    if (hit_end) *hit_end = 0;
    if (!sec || !cb) return 0;

    for (int off = 0; off < 512; off += 32) {
        const fat_dirent_t* de = (const fat_dirent_t*)(sec + off);
        char namebuf[20];

        if (de->name[0] == 0x00) {
            if (hit_end) *hit_end = 1;
            return 1;
        }

        if (de->name[0] == 0xE5) continue;
        if (de->attr == FAT_ATTR_LFN) continue;
        if (de->attr & FAT_ATTR_VOLUME_ID) continue;

        fat_build_short_name(de, namebuf, sizeof(namebuf));
        if (namebuf[0] == 0) continue;

        {
            int res = cb(de, namebuf, u);
            if (res <= 0) return res;
        }
    }

    return 1;
}

static int fat_iter_root12_16(blockdev_t* dev, const fat_info_t* info, fat_iter_cb cb, void* u)
{
    fat_public_iter_t it;
    uint32_t root_dir_start = 0;
    uint8_t sec[512];

    if (!dev || !info || !cb) return 0;
    it.cb = cb;
    it.u = u;
    root_dir_start = info->reserved_sectors + (info->fat_count * info->sectors_per_fat);

    for (uint32_t s = 0; s < info->root_dir_sectors; s++) {
        int hit_end = 0;
        int res = 0;

        if (!dev->read(dev, root_dir_start + s, 1, sec)) return 0;
        res = fat_iter_sector_entries(sec, fat_public_emit, &it, &hit_end);
        if (res <= 0) return 1;
        if (hit_end) return 1;
    }

    return 1;
}

static int fat_iter_root12_16_raw(blockdev_t* dev, const fat_info_t* info, fat_raw_iter_cb cb, void* u)
{
    uint32_t root_dir_start = 0;
    uint8_t sec[512];

    if (!dev || !info || !cb) return 0;
    root_dir_start = info->reserved_sectors + (info->fat_count * info->sectors_per_fat);

    for (uint32_t s = 0; s < info->root_dir_sectors; s++) {
        int hit_end = 0;
        int res = 0;

        if (!dev->read(dev, root_dir_start + s, 1, sec)) return 0;
        res = fat_iter_sector_entries(sec, cb, u, &hit_end);
        if (res <= 0) return 1;
        if (hit_end) return 1;
    }

    return 1;
}

static int fat_iter_dir_chain_raw(blockdev_t* dev, const fat_info_t* info, uint32_t first_cluster, fat_raw_iter_cb cb, void* u)
{
    uint8_t sec[512];
    uint32_t cluster = 0;

    if (!dev || !info || !cb || first_cluster < 2) return 0;
    cluster = first_cluster;

    while (cluster >= 2u) {
        uint32_t lba = fat_cluster_to_lba(info, cluster);
        if (lba == 0) return 0;

        for (uint32_t s = 0; s < info->sectors_per_cluster; s++) {
            int hit_end = 0;
            int res = 0;

            if (!dev->read(dev, lba + s, 1, sec)) return 0;
            res = fat_iter_sector_entries(sec, cb, u, &hit_end);
            if (res <= 0) return 1;
            if (hit_end) return 1;
        }

        cluster = fat_next_cluster(dev, info, cluster);
        if (cluster == 0xFFFFFFFFu) break;
        if (cluster < 2u) return 0;
    }

    return 1;
}

static int fat_iter_dir_chain(blockdev_t* dev, const fat_info_t* info, uint32_t first_cluster, fat_iter_cb cb, void* u)
{
    fat_public_iter_t it;

    if (!cb) return 0;
    it.cb = cb;
    it.u = u;
    return fat_iter_dir_chain_raw(dev, info, first_cluster, fat_public_emit, &it);
}

static int fat_iter_root32(blockdev_t* dev, const fat_info_t* info, fat_iter_cb cb, void* u)
{
    if (!dev || !info || !cb || info->root_cluster < 2) return 0;
    return fat_iter_dir_chain(dev, info, info->root_cluster, cb, u);
}

static int fat_find_named_entry_cb(const fat_dirent_t* de, const char* name, void* u)
{
    fat_find_ctx_t* ctx = (fat_find_ctx_t*)u;

    if (!de || !name || !ctx || !ctx->want_name) return -1;
    if (!fat_ascii_eq_icase(name, ctx->want_name)) return 1;

    ctx->out = *de;
    ctx->found = 1;
    return 0;
}

static int fat_find_in_root(blockdev_t* dev, const fat_info_t* info, const char* name, fat_dirent_t* out)
{
    fat_find_ctx_t ctx;

    if (!dev || !info || !name || !out) return 0;
    memset(&ctx, 0, sizeof(ctx));
    ctx.want_name = name;

    if (info->type == FAT_TYPE_12 || info->type == FAT_TYPE_16) {
        if (!fat_iter_root12_16_raw(dev, info, fat_find_named_entry_cb, &ctx)) return 0;
    } else if (info->type == FAT_TYPE_32) {
        if (!fat_iter_dir_chain_raw(dev, info, info->root_cluster, fat_find_named_entry_cb, &ctx)) return 0;
    } else {
        return 0;
    }

    if (!ctx.found) return 0;
    *out = ctx.out;
    return 1;
}

static int fat_find_in_dir_cluster(blockdev_t* dev, const fat_info_t* info, uint32_t dir_cluster, const char* name, fat_dirent_t* out)
{
    fat_find_ctx_t ctx;

    if (!dev || !info || !name || !out || dir_cluster < 2) return 0;
    memset(&ctx, 0, sizeof(ctx));
    ctx.want_name = name;

    if (!fat_iter_dir_chain_raw(dev, info, dir_cluster, fat_find_named_entry_cb, &ctx)) return 0;
    if (!ctx.found) return 0;

    *out = ctx.out;
    return 1;
}

static int fat_next_path_component(const char** pp, char* out, int out_sz)
{
    const char* p = 0;
    int n = 0;

    if (!pp || !out || out_sz <= 1) return 0;
    p = *pp;
    if (!p) p = "";

    while (*p == '/') p++;
    if (!*p) {
        out[0] = 0;
        *pp = p;
        return 0;
    }

    while (*p && *p != '/') {
        if (n + 1 >= out_sz) return 0;
        out[n++] = *p++;
    }

    out[n] = 0;
    while (*p == '/') p++;
    *pp = p;
    return 1;
}

static int fat_lookup_path(blockdev_t* dev, const fat_info_t* info, const char* path, fat_dirent_t* out, int* out_is_root)
{
    const char* p = path;
    uint32_t dir_cluster = 0;
    fat_dirent_t found;
    char comp[32];

    if (!dev || !info || !path) return 0;

    while (*p == '/') p++;
    if (!*p) {
        if (out_is_root) *out_is_root = 1;
        return 1;
    }

    dir_cluster = 0;
    memset(&found, 0, sizeof(found));

    while (fat_next_path_component(&p, comp, sizeof(comp))) {
        int more = (*p != 0);

        if (dir_cluster == 0) {
            if (!fat_find_in_root(dev, info, comp, &found)) return 0;
        } else {
            if (!fat_find_in_dir_cluster(dev, info, dir_cluster, comp, &found)) return 0;
        }

        if (more) {
            if (!(found.attr & FAT_ATTR_DIRECTORY)) return 0;
            dir_cluster = fat_dirent_first_cluster(&found);
            if (dir_cluster < 2u) return 0;
        }
    }

    if (out) *out = found;
    if (out_is_root) *out_is_root = 0;
    return 1;
}

int fat_probe_sector0(const uint8_t* sector, fat_info_t* out)
{
    const fat_bpb_t* bpb = 0;
    uint16_t sig = 0;
    uint32_t total_sectors = 0;
    uint32_t sectors_per_fat = 0;
    uint32_t root_dir_sectors = 0;
    uint32_t first_data_sector = 0;
    uint32_t data_sectors = 0;
    uint32_t cluster_count = 0;
    fat_type_t type = FAT_TYPE_NONE;

    if (!sector || !out) return 0;

    memset(out, 0, sizeof(*out));
    bpb = (const fat_bpb_t*)sector;
    sig = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8);

    if (sig != 0xAA55) return 0;
    if (bpb->bytes_per_sector == 0) return 0;
    if (bpb->sectors_per_cluster == 0) return 0;
    if (bpb->num_fats == 0) return 0;

    total_sectors = fat_total_sectors(bpb);
    sectors_per_fat = fat_sectors_per_fat(bpb);
    root_dir_sectors =
        ((uint32_t)bpb->root_entry_count * 32u + (bpb->bytes_per_sector - 1u)) /
        bpb->bytes_per_sector;

    first_data_sector =
        (uint32_t)bpb->reserved_sector_count +
        ((uint32_t)bpb->num_fats * sectors_per_fat) +
        root_dir_sectors;

    if (total_sectors < first_data_sector) return 0;

    data_sectors = total_sectors - first_data_sector;
    cluster_count = data_sectors / bpb->sectors_per_cluster;

    if (cluster_count < 4085u) {
        type = FAT_TYPE_12;
    } else if (cluster_count < 65525u) {
        type = FAT_TYPE_16;
    } else {
        type = FAT_TYPE_32;
    }

    out->type = type;
    out->bytes_per_sector = bpb->bytes_per_sector;
    out->sectors_per_cluster = bpb->sectors_per_cluster;
    out->reserved_sectors = bpb->reserved_sector_count;
    out->fat_count = bpb->num_fats;
    out->root_entry_count = bpb->root_entry_count;
    out->total_sectors = total_sectors;
    out->sectors_per_fat = sectors_per_fat;
    out->root_dir_sectors = root_dir_sectors;
    out->first_data_sector = first_data_sector;
    out->data_sectors = data_sectors;
    out->cluster_count = cluster_count;
    out->root_cluster = (type == FAT_TYPE_32) ? bpb->fat32.root_cluster : 0;
    return 1;
}

int fat_probe_dev(blockdev_t* dev, fat_info_t* out)
{
    uint8_t sector[512];

    if (!dev || !dev->read) return 0;
    if (dev->sector_size != 512) {
        printk("[FAT] Unsupported sector size for probe: %u\n", dev->sector_size);
        return 0;
    }

    if (!dev->read(dev, 0, 1, sector)) {
        return 0;
    }

    return fat_probe_sector0(sector, out);
}

int fat_iter_root(blockdev_t* dev, fat_iter_cb cb, void* u)
{
    fat_info_t info;

    if (!fat_probe_dev(dev, &info)) return 0;

    if (info.type == FAT_TYPE_12 || info.type == FAT_TYPE_16) {
        return fat_iter_root12_16(dev, &info, cb, u);
    }

    if (info.type == FAT_TYPE_32) {
        return fat_iter_root32(dev, &info, cb, u);
    }

    return 0;
}

int fat_iter_path(blockdev_t* dev, const char* path, fat_iter_cb cb, void* u)
{
    fat_info_t info;
    fat_dirent_t de;
    int is_root = 0;
    uint32_t cluster = 0;

    if (!cb) return 0;
    if (!fat_probe_dev(dev, &info)) return 0;
    if (!fat_lookup_path(dev, &info, path ? path : "/", &de, &is_root)) return 0;

    if (is_root) {
        if (info.type == FAT_TYPE_12 || info.type == FAT_TYPE_16) {
            return fat_iter_root12_16(dev, &info, cb, u);
        }

        if (info.type == FAT_TYPE_32) {
            return fat_iter_dir_chain(dev, &info, info.root_cluster, cb, u);
        }

        return 0;
    }

    if (!(de.attr & FAT_ATTR_DIRECTORY)) return 0;

    cluster = fat_dirent_first_cluster(&de);
    if (cluster < 2u) return 0;
    return fat_iter_dir_chain(dev, &info, cluster, cb, u);
}

int fat_stat_path(blockdev_t* dev, const char* path, uint32_t* out_size, int* out_is_dir)
{
    fat_info_t info;
    fat_dirent_t de;
    int is_root = 0;

    if (out_size) *out_size = 0;
    if (out_is_dir) *out_is_dir = 0;
    if (!fat_probe_dev(dev, &info)) return 0;
    if (!fat_lookup_path(dev, &info, path ? path : "/", &de, &is_root)) return 0;

    if (is_root) {
        if (out_is_dir) *out_is_dir = 1;
        return 1;
    }

    if (out_size) *out_size = de.file_size;
    if (out_is_dir) *out_is_dir = (de.attr & FAT_ATTR_DIRECTORY) ? 1 : 0;
    return 1;
}