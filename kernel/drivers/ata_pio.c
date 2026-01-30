#include <kernel/drivers/ata_pio.h>
#include <arch/x86/io.h>

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

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_IDENTIFY     0xEC
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_CACHE_FLUSH   0xE7   // opsiyonel ama iyi

#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01


static int g_ready = 0;

// ... 31-35. satırlar arasını (ATA_DEBUG kısmı) böyle yap:
#define ATA_DEBUG 1
#if ATA_DEBUG
  #include <kernel/printk.h>   // Değişti: ../print/printf.h yok artık
  #define ATA_LOG(...) printk(__VA_ARGS__) // Değişti: printf yerine printk
#else
  #define ATA_LOG(...) do{}while(0)
#endif

static int ata_wait_not_busy(void) {
    // basit timeout
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if ((st & ATA_SR_BSY) == 0) return 1;
    }
    return 0;
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t st = inb(ATA_IO_BASE + ATA_REG_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ)) return 1;
    }
    return 0;
}

static int ata_identify(void) {
    // Drive select: primary master (0xA0)
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, 0xA0);
    io_wait();

    // Zero registers
    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA0, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA1, 0);
    outb(ATA_IO_BASE + ATA_REG_LBA2, 0);

    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t st = inb(ATA_IO_BASE + ATA_REG_STATUS);
    ATA_LOG("[ata] status=%02x\n", st);
    if (st == 0) {
        ATA_LOG("[ata] no device on primary master\n");
        return 0;
    }

    if (!ata_wait_not_busy()) return 0;

    // Eğer LBA1/LBA2 non-zero ise ATAPI olabilir, şimdilik reject edelim
    uint8_t lba1 = inb(ATA_IO_BASE + ATA_REG_LBA1);
    uint8_t lba2 = inb(ATA_IO_BASE + ATA_REG_LBA2);
    ATA_LOG("[ata] lba1=%02x lba2=%02x\n", lba1, lba2);
    if (lba1 != 0 || lba2 != 0) return 0;

    if (!ata_wait_drq()) return 0;

    // 256 word read
    for (int i = 0; i < 256; i++) {
        (void)inw(ATA_IO_BASE + ATA_REG_DATA);
    }
    return 1;
}

static int ata_read28(uint32_t lba, uint8_t count, void* out) {
    if (count == 0) return 1;
    if (lba >= 0x10000000u) return 0; // 28-bit limit

    if (!ata_wait_not_busy()) return 0;

    // Select drive + top 4 bits of LBA
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    io_wait();

    uint16_t* dst = (uint16_t*)out;
    for (uint32_t s = 0; s < (uint32_t)count; s++) {
        if (!ata_wait_drq()) return 0;

        // 512 bytes = 256 words
        for (int i = 0; i < 256; i++) {
            dst[s * 256 + i] = inw(ATA_IO_BASE + ATA_REG_DATA);
        }
    }
    return 1;
}

static int ata_write28(uint32_t lba, uint8_t count, const void* in)
{
    if (count == 0) return 1;
    if (lba >= 0x10000000u) return 0; // 28-bit limit

    if (!ata_wait_not_busy()) return 0;

    // Select drive + top 4 bits of LBA (LBA mode)
    outb(ATA_IO_BASE + ATA_REG_HDDEVSEL, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    io_wait();

    outb(ATA_IO_BASE + ATA_REG_SECCOUNT, count);
    outb(ATA_IO_BASE + ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    io_wait();

    const uint16_t* src = (const uint16_t*)in;

    for (uint32_t s = 0; s < (uint32_t)count; s++) {
        if (!ata_wait_drq()) return 0;

        // 512 bytes = 256 words
        for (int i = 0; i < 256; i++) {
            outw(ATA_IO_BASE + ATA_REG_DATA, src[s * 256 + i]);
        }
    }

    // Cache flush (bazı disklerde önemli)
    outb(ATA_IO_BASE + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    io_wait();
    ata_wait_not_busy();

    return 1;
}

/* ---- blockdev wrapper ---- */

static int ata_dev_read(blockdev_t* d, uint64_t lba, uint32_t count, void* out) {
    (void)d;
    // Basit: count'i 255'lik parçalarla oku
    uint8_t* p = (uint8_t*)out;
    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        if (!ata_read28((uint32_t)lba, chunk, p)) return 0;
        lba += chunk;
        count -= chunk;
        p += (uint32_t)chunk * 512u;
    }
    return 1;
}

static int ata_dev_write(blockdev_t* d, uint64_t lba, uint32_t count, const void* in)
{
    (void)d;
    const uint8_t* p = (const uint8_t*)in;

    while (count > 0) {
        uint8_t chunk = (count > 255) ? 255 : (uint8_t)count;
        if (!ata_write28((uint32_t)lba, chunk, p)) return 0;
        lba += chunk;
        count -= chunk;
        p += (uint32_t)chunk * 512u;
    }
    return 1;
}

static blockdev_t g_dev = {
    .sector_size = 512,
    .user = 0,
    .read = ata_dev_read,
    .write = ata_dev_write
};

int ata_pio_init(void) {
    ATA_LOG("[ata] init...\n");
    g_ready = ata_identify();
    ATA_LOG("[ata] identify=%d\n", g_ready);
    return g_ready;
}

int ata_pio_is_ready(void) {
    return g_ready;
}

blockdev_t* ata_pio_get_dev(void) {
    if (!g_ready) return 0;
    return &g_dev;
}

/* -------- block.c compatibility wrappers -------- */
int ata_pio_read(blockdev_t* dev, uint32_t lba, void* buffer, uint32_t count) {
    if (!dev || !dev->read) return 0;
    return dev->read(dev, (uint64_t)lba, count, buffer);
}

int ata_pio_drive(blockdev_t* dev, uint32_t lba, const void* buffer, uint32_t count) {
    if (!dev || !dev->write) return 0;
    return dev->write(dev, (uint64_t)lba, count, buffer);
}
