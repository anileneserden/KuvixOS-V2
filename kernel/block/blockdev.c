#include <kernel/block/blockdev.h>

int blockdev_read(blockdev_t* d, uint64_t lba, uint32_t count, void* out)
{
    if (!d || !d->read || !out) return 0;
    if (d->sector_size == 0) return 0;
    return d->read(d, lba, count, out);
}

int blockdev_write(blockdev_t* d, uint64_t lba, uint32_t count, const void* in)
{
    if (!d || !d->write || !in) return 0;
    if (d->sector_size == 0) return 0;
    return d->write(d, lba, count, in);
}
