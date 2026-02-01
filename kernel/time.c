#include <kernel/time.h>
#include <arch/x86/io.h>
#include <kernel/drivers/rtc/rtc.h>

// --- Değişkenler ---
volatile uint32_t g_ticks_ms = 0;
uint64_t g_boot_epoch = 0;
static uint32_t g_boot_ticks_ms = 0;
int g_hour = 0, g_minute = 0, g_second = 0;
volatile int test_counter = 0;

// --- Yardımcı Zaman Fonksiyonları (En Üste Alındı) ---

static int is_leap(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

static int days_in_month(int y, int m) {
    static const int dm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && is_leap(y)) return 29;
    return dm[m-1];
}

static uint64_t days_since_epoch(int y, int m, int d) {
    uint64_t days = 0;
    for (int yr = 1970; yr < y; yr++) {
        days += is_leap(yr) ? 366 : 365;
    }
    for (int mo = 1; mo < m; mo++) {
        days += (uint64_t)days_in_month(y, mo);
    }
    days += (uint64_t)(d - 1);
    return days;
}

// Hata veren fonksiyon buydu, artık yukarıda olduğu için sorun çıkmayacak
static uint64_t datetime_to_epoch(const rtc_datetime_t* t) {
    uint64_t days = days_since_epoch((int)t->year, (int)t->month, (int)t->day);
    uint64_t sec  = days * 86400ULL;
    sec += (uint64_t)t->hour * 3600ULL;
    sec += (uint64_t)t->min  * 60ULL;
    sec += (uint64_t)t->sec;
    return sec;
}

static rtc_datetime_t epoch_to_datetime(uint64_t epoch) {
    rtc_datetime_t out;
    uint64_t days = epoch / 86400ULL;
    uint64_t sod  = epoch % 86400ULL;

    out.hour = (uint8_t)(sod / 3600ULL);
    sod %= 3600ULL;
    out.min  = (uint8_t)(sod / 60ULL);
    out.sec  = (uint8_t)(sod % 60ULL);

    int y = 1970;
    while (1) {
        uint64_t yd = (uint64_t)(is_leap(y) ? 366 : 365);
        if (days >= yd) { days -= yd; y++; }
        else break;
    }

    int m = 1;
    while (1) {
        int md = days_in_month(y, m);
        if (days >= (uint64_t)md) { days -= (uint64_t)md; m++; }
        else break;
    }

    out.year  = (uint16_t)y;
    out.month = (uint8_t)m;
    out.day   = (uint8_t)(days + 1);

    return out;
}

// --- Ana Fonksiyonlar ---

void timer_init(uint32_t freq) {
    uint32_t divisor = 1193182 / freq;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void timer_handler(void) {
    g_ticks_ms++;
    if (g_ticks_ms % 1000 == 0) {
        test_counter++;
        g_second++;
        if (g_second >= 60) {
            g_second = 0;
            g_minute++;
            if (g_minute >= 60) {
                g_minute = 0;
                g_hour++;
                if (g_hour >= 24) g_hour = 0;
            }
        }
    }
    outb(0x20, 0x20);
}

void time_init_from_rtc(void) {
    rtc_datetime_t t;
    if (rtc_read_datetime(&t)) {
        // Artık datetime_to_epoch yukarıda tanımlı olduğu için derleyici hata vermez
        g_boot_epoch = datetime_to_epoch(&t);
        g_hour = t.hour;
        g_minute = t.min;
        g_second = t.sec;
    }
    g_boot_ticks_ms = g_ticks_ms;
}

uint64_t time_now_epoch_sec(void) {
    uint32_t dt_ms = g_ticks_ms - g_boot_ticks_ms;
    return g_boot_epoch + (uint64_t)(dt_ms / 1000U);
}

rtc_datetime_t time_now_datetime(void) {
    return epoch_to_datetime(time_now_epoch_sec());
}