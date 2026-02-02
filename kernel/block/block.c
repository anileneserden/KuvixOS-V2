#include <kernel/block/block.h>
#include <kernel/block/blockdev.h>
#include <kernel/drivers/virtio_blk.h>
#include <kernel/drivers/ata_pio.h>
#include <kernel/printk.h>

// küçük helper (blockdev.c fonksiyonlarını burada forward ediyoruz)
int blockdev_read(blockdev_t* d, uint64_t lba, uint32_t count, void* out);
int blockdev_write(blockdev_t* d, uint64_t lba, uint32_t count, const void* in);

static blockdev_t* g_root = 0;

void block_set_root(blockdev_t* dev)
{
    g_root = dev;
}

blockdev_t* block_get_root(void)
{
    return g_root;
}

int block_has_root(void)
{
    return g_root != 0;
}

void block_init(void)
{
    g_root = 0;

    // 1) VirtIO dene
    if (virtio_blk_init()) {
        blockdev_t* v = virtio_blk_get_dev();
        if (v) { g_root = v; return; }
    }

    // 2) ATA PIO dene (QEMU if=ide -> burası)
    if (ata_pio_init()) {
        blockdev_t* a = ata_pio_get_dev();
        if (a) { g_root = a; return; }
    }

    // root yok
}

int block_read(uint64_t lba, uint32_t count, void* out)
{
    if (!g_root) return 0;
    return blockdev_read(g_root, lba, count, out);
}

int block_write(uint64_t lba, uint32_t count, const void* in)
{
    if (!g_root) {
        printk("BLOCK: HATA - g_root null! Disk bagli degil.\n");
        return 0;
    }
    int res = blockdev_write(g_root, lba, count, in);
    if (!res) {
        printk("BLOCK: HATA - blockdev_write basarisiz (LBA: %d)!\n");
    }
    return res;
}