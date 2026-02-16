#include <kernel/fs/fat32.h>
#include <kernel/printk.h>
#include <lib/string.h>

static uint16_t rd16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

uint32_t fat32_cluster_to_lba(const fat32_t* fs, uint32_t cluster) {
    // cluster 2 = ilk data cluster
    return fs->data_lba + (cluster - 2u) * (uint32_t)fs->sectors_per_cluster;
}

int fat32_mount(blockdev_t* dev, uint32_t part_lba, fat32_t* out) {
    if (!dev || !dev->read || !out) return 0;

    printk("[fat32] try read boot sector at LBA=%d\n", (int)part_lba);

    uint8_t bs[512];

    int ok = dev->read(dev, (uint64_t)part_lba, 1, bs);
    printk("[fat32] read ret=%d\n", ok);

    if (!ok) {
        printk("[fat32] boot sector read failed\n");
        return 0;
    }

    printk("[fat32] sig raw=0x%x 0x%x\n", (unsigned)bs[510], (unsigned)bs[511]);

    // 0x55AA signature
    if (bs[510] != 0x55 || bs[511] != 0xAA) {
        printk("[fat32] invalid VBR signature (0x%02x%02x)\n", bs[511], bs[510]);
        return 0;
    }

    uint16_t bps = rd16(&bs[11]);
    uint8_t  spc = bs[13];
    uint16_t rsv = rd16(&bs[14]);
    uint8_t  nf  = bs[16];
    uint16_t root_ent_cnt = rd16(&bs[17]);
    uint16_t tot16 = rd16(&bs[19]);
    uint32_t tot32 = rd32(&bs[32]);
    uint16_t fatsz16 = rd16(&bs[22]);
    uint32_t fatsz32 = rd32(&bs[36]);
    uint32_t rootclus = rd32(&bs[44]);

    if (bps != 512) {
        printk("[fat32] bytes/sector=%d (512 bekleniyor)\n", bps);
        return 0;
    }

    if (spc == 0 || nf == 0) {
        printk("[fat32] invalid spc/nf\n");
        return 0;
    }

    if (root_ent_cnt != 0 || fatsz16 != 0 || fatsz32 == 0) {
        printk("[fat32] not FAT32? rootEnt=%d fatsz16=%d fatsz32=%d\n",
               root_ent_cnt, fatsz16, fatsz32);
        return 0;
    }

    memset(out, 0, sizeof(*out));

    out->part_lba = part_lba;
    out->bytes_per_sector = bps;
    out->sectors_per_cluster = spc;
    out->reserved_sectors = rsv;
    out->num_fats = nf;
    out->fat_size_sectors = fatsz32;
    out->root_cluster = (rootclus ? rootclus : 2);

    out->fat_lba  = part_lba + (uint32_t)rsv;
    out->data_lba = out->fat_lba + (uint32_t)nf * fatsz32;

    uint32_t total_secs = tot16 ? (uint32_t)tot16 : tot32;

    printk("[fat32] mounted:\n");
    printk("  bps=%d spc=%d rsv=%d fats=%d fatsz=%d\n",
           (int)out->bytes_per_sector,
           (int)out->sectors_per_cluster,
           (int)out->reserved_sectors,
           (int)out->num_fats,
           (int)out->fat_size_sectors);
    printk("  rootclus=%d total=%d\n",
           (int)out->root_cluster,
           (int)total_secs);
    printk("  fat_lba=%d data_lba=%d\n",
           (int)out->fat_lba,
           (int)out->data_lba);

    return 1;
}
