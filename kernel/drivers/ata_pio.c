#include <kernel/drivers/ata_pio.h>
#include <arch/x86/io.h>
#include <kernel/printk.h>
#include <stdint.h>

#define ATA_IO_BASE   0x1F0
#define ATA_CTRL_BASE 0x3F6

#define ATA_REG_DATA      0
#define ATA_REG_ERROR     1
#define ATA_REG_SECCOUNT  2
#define ATA_REG_LBA0      3
#define ATA_REG_LBA1      4
#define ATA_REG_LBA2      5
#define ATA_REG_HDDEVSEL  6
#define ATA_REG_STATUS    7
#define ATA_REG_COMMAND   7

#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH   0xE7

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

// 0 = Primary Master, 1 = Primary Slave
static int g_ready0 = 0;
static int g_ready1 = 0;

#define ATA_LOG(...) printk(__VA_ARGS__)

// Diskin meşguliyetinin bitmesini bekler
static int ata_wait_not_busy(void) {
    // Döngü sayısını 100.000'den 500.000'e çıkar
    for (int i = 0; i < 2000000; i++) {
        uint8_t status = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) return 1;
    }
    return 0;
}

// Diskin veri transferine hazır olmasını bekler
static int ata_wait_drq(void) {
    for (int i = 0; i < 2000000; i++) {
        uint8_t status = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if (status & ATA_SR_DRQ) return 1;
        if (status & ATA_SR_ERR) return 0;
    }
    return 0;
}

static int ata_identify_drive(int drive) {
    // drive: 0=master (0xA0), 1=slave (0xB0)
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (drive ? 0x10 : 0x00)));
    io_wait();

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA0, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA1, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA2, 0);

    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t st = inb(ATA_IO_BASE + ATA_REG_STATUS);
    if (st == 0) return 0; // Cihaz yok

    if (!ata_wait_not_busy()) return 0;

    uint8_t lba1 = inb(ATA_IO_BASE + ATA_REG_LBA1);
    uint8_t lba2 = inb(ATA_IO_BASE + ATA_REG_LBA2);
    if (lba1 != 0 || lba2 != 0) return 0; // ATAPI cihaz (CDROM vb.) istemiyoruz

    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++) {
        (void)inw(ATA_IO_BASE + ATA_REG_DATA);
    }
    
    ata_wait_not_busy();
    return 1;
}

static int ata_read28(int drive, uint32_t lba, uint8_t count, void* out) {
    if (count == 0) return 1;
    if (!ata_wait_not_busy()) return 0;

    // 0xE0  = master LBA, 0xF0 = slave LBA
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL,
         (uint8_t)((drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F)));
    io_wait();

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    uint16_t* dst = (uint16_t*)out;
    for (uint32_t s = 0; s < count; s++) {
        if (!ata_wait_drq()) return 0;
        for (int i = 0; i < 256; i++) {
            *dst++ = inw(ATA_IO_BASE + ATA_REG_DATA);
        }
    }
    return 1;
}

static int ata_write28(int drive, uint32_t lba, uint8_t count, const void* in) {
    if (count == 0) return 1;
    if (!ata_wait_not_busy()) return 0;

    // Sürücü ve LBA yüksek bitlerini seç
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL,
          (uint8_t)((drive ? 0xF0 : 0xE0) | ((lba >> 24) & 0x0F)));
    
    // Seçim sonrası kontrolcünün kendine gelmesi için 400ns (4 okuma) bekle
    for(int i=0; i<4; i++) inb(ATA_IO_BASE + ATA_REG_STATUS);

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)lba);
    outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint16_t* ptr = (const uint16_t*)in;
    for (int i = 0; i < count; i++) {
        // Disk "veri gönderebilirsin" diyene kadar bekle
        if (!ata_wait_drq()) {
            printk("ATA: Yazma hatasi - DRQ zaman asimi!\n");
            return 0;
        }

        for (int j = 0; j < 256; j++) {
            outw(ATA_IO_BASE + ATA_REG_DATA, ptr[i * 256 + j]);
        }
        
        // Her sektörden sonra kontrolcünün veriyi işlemesine izin ver
        for(int n=0; n<4; n++) inb(ATA_IO_BASE + ATA_REG_STATUS);
    }
    
    // Flush komutu (0xE7) gönderilmeli
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);

    // 400ns bekleme
    for(int n = 0; n < 4; n++) {
        inb(ATA_IO_BASE + ATA_REG_STATUS);
    }
    
    // BURASI ÇOK ÖNEMLİ: Sonucu döndür
    return ata_wait_not_busy();
}

/* ---- blockdev interface ---- */

static int ata_dev_read(blockdev_t* d, uint64_t lba, uint32_t count, void* out) {
    int drive = (int)(uintptr_t)d->user;
    uint8_t* p = (uint8_t*)out;
    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        if (!ata_read28(drive, (uint32_t)lba, chunk, p)) return 0;
        lba += chunk;
        count -= chunk;
        p += (uint32_t)chunk * 512u;
    }
    return 1;
}

static int ata_dev_write(blockdev_t* d, uint64_t lba, uint32_t count, const void* in) {
    int drive = (int)(uintptr_t)d->user;
    const uint8_t* p = (const uint8_t*)in;
    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        if (!ata_write28(drive, (uint32_t)lba, chunk, p)) return 0;
        lba += chunk;
        count -= chunk;
        p += (uint32_t)chunk * 512u;
    }
    return 1;
}

static blockdev_t g_dev0 = {
    .sector_size = 512,
    .user = (void*)0,
    .read = ata_dev_read,
    .write = ata_dev_write
};

static blockdev_t g_dev1 = {
    .sector_size = 512,
    .user = (void*)1,
    .read = ata_dev_read,
    .write = ata_dev_write
};

int ata_pio_init(void) {
    ATA_LOG("[ata] init...\n");
    g_ready0 = ata_identify_drive(0);
    g_ready1 = ata_identify_drive(1);
    ATA_LOG("[ata] identify: master=%d slave=%d\n", g_ready0, g_ready1);
    return (g_ready0 || g_ready1);
}

int ata_pio_is_ready(void) { return g_ready0; }
int ata_pio_is_ready2(void) { return g_ready1; }

blockdev_t* ata_pio_get_dev(void) { return g_ready0 ? &g_dev0 : 0; }
blockdev_t* ata_pio_get_dev2(void) { return g_ready1 ? &g_dev1 : 0; }

// block.h uyumluluk fonksiyonları
int ata_pio_read(blockdev_t* dev, uint32_t lba, void* buffer, uint32_t count) {
    return ata_dev_read(dev, (uint64_t)lba, count, buffer);
}

int ata_pio_drive(blockdev_t* dev, uint32_t lba, const void* buffer, uint32_t count) {
    return ata_dev_write(dev, (uint64_t)lba, count, buffer);
}

// Diskin toplam sektör sayısını ve MB cinsinden boyutunu yazdırır
void ata_pio_print_info(void) {
    if (!g_ready0 && !g_ready1) {
        printk("  [ATA] Bagli degil veya hazir degil\n");
        return;
    }

    if (g_ready0) printk("  0: [ATA] Primary Master - Durum: Hazir\n");
    else          printk("  0: [ATA] Primary Master - Durum: Yok\n");

    if (g_ready1) printk("  1: [ATA] Primary Slave  - Durum: Hazir\n");
    else          printk("  1: [ATA] Primary Slave  - Durum: Yok\n");
}