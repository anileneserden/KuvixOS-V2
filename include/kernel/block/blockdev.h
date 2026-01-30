#pragma once
#include <stdint.h>

typedef struct blockdev blockdev_t;

typedef int (*block_read_fn)(blockdev_t* d, uint64_t lba, uint32_t count, void* out);
typedef int (*block_write_fn)(blockdev_t* d, uint64_t lba, uint32_t count, const void* in);

struct blockdev {
    uint32_t sector_size;   // 512 bekliyoruz
    void*    user;          // driver private
    block_read_fn  read;
    block_write_fn write;   // yoksa 0 olabilir
};
