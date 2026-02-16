#pragma once
#include <stdint.h>
#include <kernel/block/blockdev.h>

int         ata_pio_init(void);
int         ata_pio_is_ready(void);
blockdev_t* ata_pio_get_dev(void);
void        ata_pio_print_info(void);
int         ata_pio_is_ready2(void);
blockdev_t* ata_pio_get_dev2(void);
int         ata_pio_read(blockdev_t* dev, uint32_t lba, void* buffer, uint32_t count);
int         ata_pio_drive(blockdev_t* dev, uint32_t lba, const void* buffer, uint32_t count);