#pragma once
#include <stdint.h>
#include "blockdev.h"

// block subsystem
void block_init(void);

// root block device (VirtIO vb.) set/get
void block_set_root(blockdev_t* dev);
blockdev_t* block_get_root(void);
int block_has_root(void);

// wrappers (root üzerinden)
int block_read(uint64_t lba, uint32_t count, void* out);
int block_write(uint64_t lba, uint32_t count, const void* in);
