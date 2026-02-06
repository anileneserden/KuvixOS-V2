#ifndef ATA_PIO_H
#define ATA_PIO_H

#include <stdint.h>
#include <stdbool.h>
#include <kernel/block/blockdev.h>

// Disk yapısı
typedef struct {
    uint16_t base;
    uint8_t drive;
    bool present;
    char model[41];
} ata_disk_t;

// --- FONKSİYON BİLDİRİMLERİ ---
void ata_pio_scan_all(void);
int ata_pio_init(void);
int ata_pio_is_ready(void);
int ata_pio_get_disk_count(void);
ata_disk_t* ata_pio_get_disk(int index);
blockdev_t* ata_pio_get_dev(void);

// Okuma/Yazma Fonksiyonları
int ata_pio_read_disk(ata_disk_t* disk, uint32_t lba, void* out);
int ata_pio_write_disk(ata_disk_t* disk, uint32_t lba, const void* in);

// Köprü Fonksiyonlar (Eski kodlar için)
int ata_pio_read(blockdev_t* dev, uint32_t lba, void* buffer, uint32_t count);
int ata_pio_drive(blockdev_t* dev, uint32_t lba, const void* buffer, uint32_t count);

// BU SATIRI EKLE:
void ata_pio_print_info(void);

#endif