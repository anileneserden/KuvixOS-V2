#pragma once
#include <stdint.h>

typedef enum {
    POWER_OK = 0,
    POWER_FAILED = -1
} power_result_t;

// Sistemi kapatmayı dener (QEMU/Bochs/ACPI denemeleri)
power_result_t power_shutdown(void);

// Sistemi yeniden başlatmayı dener (8042 + triple fault fallback)
power_result_t power_reboot(void);

// CPU'yu durdur (fallback)
void power_halt(void);
