#include <kernel/drivers/rtc/rtc.h>
#include <arch/x86/io.h>

static inline uint8_t rtc_read_reg(uint8_t reg) {
    // NMI disable (bit7=1) + reg
    outb(0x70, (uint8_t)(0x80 | reg));
    return inb(0x71);
}

static inline int rtc_uip(void) {
    // Update-in-progress bit = bit7 of Status A (0x0A)
    return (rtc_read_reg(0x0A) & 0x80) ? 1 : 0;
}

static inline uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

// Güvenli okuma: UIP yokken 2 kez oku, aynıysa kabul et.
int rtc_read_datetime(rtc_datetime_t* out) {
    if (!out) return 0;

    // UIP bitinin 0 olmasını bekle
    for (int i = 0; i < 100000; i++) {
        if (!rtc_uip()) break;
    }

    // StatusB: BCD mi? 24h mi?
    uint8_t statusB = rtc_read_reg(0x0B);
    int is_binary = (statusB & 0x04) ? 1 : 0;
    int is_24h    = (statusB & 0x02) ? 1 : 0;

    // 2 kez oku, tutarlı olana kadar dene
    uint8_t sec1, min1, hour1, day1, mon1, year1;
    uint8_t sec2, min2, hour2, day2, mon2, year2;

    for (int tries = 0; tries < 10; tries++) {
        while (rtc_uip()) { /* wait */ }

        sec1  = rtc_read_reg(0x00);
        min1  = rtc_read_reg(0x02);
        hour1 = rtc_read_reg(0x04);
        day1  = rtc_read_reg(0x07);
        mon1  = rtc_read_reg(0x08);
        year1 = rtc_read_reg(0x09);

        while (rtc_uip()) { /* wait */ }

        sec2  = rtc_read_reg(0x00);
        min2  = rtc_read_reg(0x02);
        hour2 = rtc_read_reg(0x04);
        day2  = rtc_read_reg(0x07);
        mon2  = rtc_read_reg(0x08);
        year2 = rtc_read_reg(0x09);

        if (sec1 == sec2 && min1 == min2 && hour1 == hour2 &&
            day1 == day2 && mon1 == mon2 && year1 == year2) {
            break;
        }

        if (tries == 9) return 0;
    }

    uint8_t sec  = sec2;
    uint8_t min  = min2;
    uint8_t hour = hour2;
    uint8_t day  = day2;
    uint8_t mon  = mon2;
    uint8_t yr   = year2;

    // BCD ise dönüştür
    if (!is_binary) {
        sec  = bcd_to_bin(sec);
        min  = bcd_to_bin(min);
        // hour'da 12h modunda PM biti olabilir (bit7)
        uint8_t hour_raw = hour;
        hour = bcd_to_bin((uint8_t)(hour_raw & 0x7F));
        day  = bcd_to_bin(day);
        mon  = bcd_to_bin(mon);
        yr   = bcd_to_bin(yr);

        // 12h modunda PM düzelt
        if (!is_24h) {
            int pm = (hour_raw & 0x80) ? 1 : 0;
            if (pm && hour < 12) hour = (uint8_t)(hour + 12);
            if (!pm && hour == 12) hour = 0; // 12AM -> 00
        }
    } else {
        // binary ama 12h modunda PM biti yine olabilir
        if (!is_24h) {
            int pm = (hour & 0x80) ? 1 : 0;
            hour = (uint8_t)(hour & 0x7F);
            if (pm && hour < 12) hour = (uint8_t)(hour + 12);
            if (!pm && hour == 12) hour = 0;
        }
    }

    // Yıl: basit yaklaşım (2000+)
    // İstersen century reg (0x32) destekleriz ama QEMU için çoğunlukla yeterli.
    out->year  = (uint16_t)(2000 + yr);
    out->month = mon;
    out->day   = day;
    out->hour  = hour;
    out->min   = min;
    out->sec   = sec;

    return 1;
}
