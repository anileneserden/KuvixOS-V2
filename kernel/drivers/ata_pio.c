#include <kernel/drivers/ata_pio.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>
#include <lib/string.h>

/* --- Port Tanımlamaları --- */
#define ATA_REG_DATA      0
#define ATA_REG_ERROR     1
#define ATA_REG_SECCOUNT  2
#define ATA_REG_LBA0      3
#define ATA_REG_LBA1      4
#define ATA_REG_LBA2      5
#define ATA_REG_HDDEVSEL  6
#define ATA_REG_STATUS    7
#define ATA_REG_COMMAND   7

/* --- Komutlar ve Bayraklar --- */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_CACHE_FLUSH   0xE7

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* --- Global Değişkenler --- */
static ata_disk_t g_disks[4];
static int g_disk_count = 0;
static int g_ready = 0;

/* --- Yardımcı Fonksiyonlar --- */

static int ata_wait_not_busy(uint16_t base) {
    for (int i = 0; i < 1000000; i++) {
        if (!(inb(base + ATA_REG_STATUS) & ATA_SR_BSY)) return 1;
    }
    return 0;
}

static int ata_wait_drq(uint16_t base) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t status = inb(base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return 0;
        if (status & ATA_SR_DRQ) return 1;
    }
    return 0;
}

static void ata_fix_string(char* str, int len) {
    for (int i = 0; i < len; i += 2) {
        char tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }
    for (int i = len - 1; i >= 0 && str[i] == ' '; i--) str[i] = '\0';
}

/* --- Alt Seviye Okuma/Yazma --- */

int ata_pio_read_disk(ata_disk_t* disk, uint32_t lba, void* out) {
    uint16_t base = disk->base;
    if (!ata_wait_not_busy(base)) return 0;

    // 0xE0 (Master) veya 0xF0 (Slave) kullanarak LBA bayrağını (bit 6) set ediyoruz
    outb(base + ATA_REG_HDDEVSEL, (disk->drive == 0xA0 ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(base + ATA_REG_SECCOUNT, 1);
    outb(base + ATA_REG_LBA0, (uint8_t)lba);
    outb(base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq(base)) return 0;

    uint16_t* dst = (uint16_t*)out;
    for (int i = 0; i < 256; i++) *dst++ = inw(base + ATA_REG_DATA);
    
    return 1;
}

int ata_pio_write_disk(ata_disk_t* disk, uint32_t lba, const void* in) {
    uint16_t base = disk->base;
    if (!ata_wait_not_busy(base)) return 0;

    // VirtualBox için en güvenli drive select formatı
    outb(base + ATA_REG_HDDEVSEL, (disk->drive == 0xA0 ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(base + ATA_REG_SECCOUNT, 1);
    outb(base + ATA_REG_LBA0, (uint8_t)lba);
    outb(base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_drq(base)) return 0;

    const uint16_t* src = (const uint16_t*)in;
    for (int i = 0; i < 256; i++) outw(base + ATA_REG_DATA, src[i]);

    // Veriyi diske kalıcı olarak işle (VirtualBox için kritik)
    outb(base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    
    // 400ns kuralı: Durum kaydını 4 kez oku
    for(int i = 0; i < 4; i++) inb(base + ATA_REG_STATUS);

    return ata_wait_not_busy(base);
}

/* --- Block Katmanı Arayüzü --- */

static int ata_dev_read(blockdev_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    (void)dev;
    if (g_disk_count == 0) return 0;
    uint8_t* p = (uint8_t*)buffer;
    for(uint32_t i=0; i<count; i++) {
        if(!ata_pio_read_disk(&g_disks[0], (uint32_t)lba + i, p + (i * 512))) return 0;
    }
    return 1;
}

static int ata_dev_write(blockdev_t* dev, uint64_t lba, uint32_t count, const void* buffer) {
    (void)dev;
    if (g_disk_count == 0) return 0;
    const uint8_t* p = (const uint8_t*)buffer;
    for(uint32_t i=0; i<count; i++) {
        if(!ata_pio_write_disk(&g_disks[0], (uint32_t)lba + i, p + (i * 512))) return 0;
    }
    return 1;
}

static blockdev_t g_dev = {
    .sector_size = 512,
    .read = ata_dev_read,
    .write = ata_dev_write
};

/* --- Public API --- */

void ata_pio_scan_all(void) {
    g_disk_count = 0;
    uint16_t ports[] = {0x1F0, 0x170}; 
    uint8_t drives[] = {0xA0, 0xB0};   

    for (int p = 0; p < 2; p++) {
        for (int d = 0; d < 2; d++) {
            uint16_t base = ports[p];
            uint8_t drive = drives[d];

            outb(base + ATA_REG_HDDEVSEL, drive);
            for(int i=0; i<4; i++) inb(base + ATA_REG_STATUS); 

            outb(base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            
            uint8_t status = inb(base + ATA_REG_STATUS);
            if (status == 0xFF || status == 0) continue; 

            if (ata_wait_not_busy(base)) {
                uint8_t l1 = inb(base + ATA_REG_LBA1);
                uint8_t l2 = inb(base + ATA_REG_LBA2);

                if (l1 == 0 && l2 == 0) {
                    if (ata_wait_drq(base)) {
                        uint16_t data[256];
                        for (int i = 0; i < 256; i++) data[i] = inw(base + ATA_REG_DATA);

                        g_disks[g_disk_count].base = base;
                        g_disks[g_disk_count].drive = drive;
                        g_disks[g_disk_count].present = true;

                        memcpy(g_disks[g_disk_count].model, (char*)(data + 27), 40);
                        g_disks[g_disk_count].model[40] = '\0';
                        ata_fix_string(g_disks[g_disk_count].model, 40);

                        g_disk_count++;
                    }
                }
            }
        }
    }
    g_ready = (g_disk_count > 0);
}

int ata_pio_init(void) {
    ata_pio_scan_all();
    return g_ready;
}

int ata_pio_is_ready(void) { return g_ready; }
int ata_pio_get_disk_count(void) { return g_disk_count; }
ata_disk_t* ata_pio_get_disk(int index) {
    if (index >= 0 && index < g_disk_count) return &g_disks[index];
    return 0;
}
blockdev_t* ata_pio_get_dev(void) { return g_ready ? &g_dev : 0; }

void ata_pio_print_info(void) {
    if (g_disk_count == 0) return;
    for (int i = 0; i < g_disk_count; i++) {
        printk("Disk %d: %s [%s]\n", i, g_disks[i].model, 
               (g_disks[i].drive == 0xA0 ? "Master" : "Slave"));
    }
}

int ata_pio_read(blockdev_t* dev, uint32_t lba, void* buffer, uint32_t count) {
    (void)dev;
    if (g_disk_count == 0) return 0;
    uint8_t* p = (uint8_t*)buffer;
    for(uint32_t i = 0; i < count; i++) {
        if(!ata_pio_read_disk(&g_disks[0], lba + i, p + (i * 512))) return 0;
    }
    return 1;
}