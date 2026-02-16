#include <kernel/printk.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/fs/fat32.h>
#include <lib/commands.h>

void cmd_fatinfo(int argc, char** argv){
    (void)argc; (void)argv;

    blockdev_t* dev = ata_pio_get_dev();
    if (!dev) { printk("Disk yok.\n"); return; }

    fat32_t fs;
    if (!fat32_mount(dev, 2048, &fs)) {
        printk("fat32 mount fail\n");
        return;
    }

    printk("fat_lba=%d data_lba=%d root_cluster=%d\n",
       (int)fs.fat_lba, (int)fs.data_lba, (int)fs.root_cluster);
}

REGISTER_COMMAND(fatinfo, cmd_fatinfo, "FAT32 boot sector bilgisi (p1=2048)");
