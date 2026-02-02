#pragma once
#include <stdint.h>
#include "blockdev.h"

// RAM üstünde block device
// mem: backing buffer, size: byte
void ramdisk_init(void* mem, uint32_t size);
blockdev_t* ramdisk_get_dev(void);
