#include <kernel/time.h>

// sende zaten var:
uint32_t g_ticks_ms = 0;
uint64_t g_boot_epoch = 0;      // boot anındaki gerçek zaman (epoch)

static uint32_t g_boot_ticks_ms = 0;   // boot anındaki tick

static int is_leap(int y) {
    // gregorian
    if ((y % 4) != 0) return 0;
    if ((y % 100) != 0) return 1;
    return (y % 400) == 0;
}

static int days_in_month(int y, int m) {
    static const int dm[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int d = dm[m-1];
    if (m == 2 && is_leap(y)) d = 29;
    return d;
}

// 1970-01-01'den itibaren gün sayısı
static uint64_t days_since_epoch(int y, int m, int d) {
    // y: full year, m:1-12, d:1-31
    uint64_t days = 0;

    // years
    for (int yr = 1970; yr < y; yr++) {
        days += is_leap(yr) ? 366 : 365;
    }

    // months
    for (int mo = 1; mo < m; mo++) {
        days += (uint64_t)days_in_month(y, mo);
    }

    // days in month
    days += (uint64_t)(d - 1);
    return days;
}

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

void time_init_from_rtc(void) {
    rtc_datetime_t t;
    if (rtc_read_datetime(&t)) {
        g_boot_epoch = datetime_to_epoch(&t);
    } else {
        // RTC okunamazsa 0'a düş (en azından crash olmasın)
        g_boot_epoch = 0;
    }
    g_boot_ticks_ms = g_ticks_ms;
}

uint64_t time_now_epoch_sec(void) {
    uint32_t dt_ms = g_ticks_ms - g_boot_ticks_ms;
    uint64_t dt_s  = (uint64_t)(dt_ms / 1000U);
    return g_boot_epoch + dt_s;
}

rtc_datetime_t time_now_datetime(void) {
    return epoch_to_datetime(time_now_epoch_sec());
}

static void two_digits(char* p, uint8_t v) {
    p[0] = (char)('0' + (v / 10));
    p[1] = (char)('0' + (v % 10));
}

void time_format_hhmm(char* out6) {
    rtc_datetime_t t = time_now_datetime();
    two_digits(&out6[0], t.hour);
    out6[2] = ':';
    two_digits(&out6[3], t.min);
    out6[5] = 0;
}

void time_format_hhmmss(char* out9) {
    rtc_datetime_t t = time_now_datetime();
    two_digits(&out9[0], t.hour);
    out9[2] = ':';
    two_digits(&out9[3], t.min);
    out9[5] = ':';
    two_digits(&out9[6], t.sec);
    out9[8] = 0;
}
