#pragma once
#include <stdint.h>

typedef struct {
    uint16_t year;  // e.g. 2025
    uint8_t  month; // 1-12
    uint8_t  day;   // 1-31
    uint8_t  hour;  // 0-23
    uint8_t  min;   // 0-59
    uint8_t  sec;   // 0-59
} rtc_datetime_t;

// RTC'den (CMOS) tarih/saat oku. Başarılıysa 1, değilse 0 döndür.
int rtc_read_datetime(rtc_datetime_t* out);
