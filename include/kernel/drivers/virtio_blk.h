#pragma once
#include <stdint.h>
#include <kernel/block/blockdev.h>

// init: aygıt bulduysa 1, yoksa 0
int virtio_blk_init(void);
int virtio_blk_is_ready(void);

int virtio_blk_read_sectors(uint64_t lba, uint32_t count, void* out);

blockdev_t* virtio_blk_get_dev(void);
