#include <kernel/drivers/virtio_blk.h>

static int g_ready = 0;

int virtio_blk_init(void)
{
    // TODO: PCI scan + virtio-blk init
    g_ready = 0;
    return 0;
}

int virtio_blk_is_ready(void)
{
    return g_ready;
}

int virtio_blk_read_sectors(uint64_t lba, uint32_t count, void* out)
{
    (void)lba; (void)count; (void)out;
    return 0;
}

static int vblk_read(blockdev_t* d, uint64_t lba, uint32_t count, void* out)
{
    (void)d;
    return virtio_blk_read_sectors(lba, count, out);
}

static int vblk_write(blockdev_t* d, uint64_t lba, uint32_t count, const void* in)
{
    (void)d; (void)lba; (void)count; (void)in;
    return 0;
}

static blockdev_t g_dev = {
    .sector_size = 512,
    .user = 0,
    .read = vblk_read,
    .write = vblk_write
};

blockdev_t* virtio_blk_get_dev(void)
{
    if (!virtio_blk_is_ready()) return 0;
    return &g_dev;
}
