#pragma once
#include <stdint.h>
#include <kernel/drivers/rtc/rtc.h>

// DEĞİŞİKLİK: Buradaki 'static' tanımları sil, yerine bunları yaz:
extern volatile uint32_t g_ticks_ms;
extern uint64_t g_boot_epoch;

void time_init_from_rtc(void);
uint64_t time_now_epoch_sec(void);
rtc_datetime_t time_now_datetime(void);
void time_format_hhmm(char* out6);
void time_format_hhmmss(char* out9);
void format_uptime_simple(char* out, uint32_t h, uint32_t m, uint32_t s);