#include <kernel/printk.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/block/blockdev.h>
#include <lib/commands.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t sectors;
} mbr_part_t;

void cmd_parts(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    blockdev_t* dev = ata_pio_get_dev();
    if (!dev) dev = ata_pio_get_dev2();
    if (!dev) { printk("Disk bulunamadı.\n"); return; }

    uint8_t sector[512];

    if (!dev->read(dev, 0, 1, sector)) {
        printk("MBR okunamadı.\n");
        return;
    }

    // signature kontrol
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        printk("Geçersiz MBR signature.\n");
        return;
    }

    printk("MBR bulundu.\n");
    printk("--------------------------------------\n");

    mbr_part_t* parts = (mbr_part_t*)(sector + 0x1BE);

    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0)
            continue;

        uint32_t mb = parts[i].sectors / 2048; // ~MB

        printk("p%d  type=0x%x  lba=%d  sectors=%d  (~%d MB)\n",
               i + 1,
               parts[i].type,
               parts[i].lba_start,
               parts[i].sectors,
               mb);
    }

    printk("--------------------------------------\n");
}

REGISTER_COMMAND(parts, cmd_parts, "MBR partition tablosunu gösterir");
