// include/kernel/time.h
#pragma once
#include <stdint.h>
#include <kernel/drivers/rtc/rtc.h>

extern volatile uint32_t g_ticks_ms;
extern uint64_t g_boot_epoch;

// timezone (seconds)
void time_set_tz_offset_sec(int32_t off_sec);
int32_t time_get_tz_offset_sec(void);

void timer_init(uint32_t freq);
void timer_handler(void);

void time_init_from_rtc(void);

uint64_t time_now_epoch_sec(void);
rtc_datetime_t time_now_datetime(void);
rtc_datetime_t time_now_datetime_local(void);

void time_format_hhmm(char* out6);
void time_format_hhmmss(char* out9);