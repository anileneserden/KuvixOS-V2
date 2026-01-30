#pragma once
#include <stdint.h>
#include <kernel/block/blockdev.h>

int        ata_pio_init(void);
int        ata_pio_is_ready(void);
blockdev_t* ata_pio_get_dev(void);
