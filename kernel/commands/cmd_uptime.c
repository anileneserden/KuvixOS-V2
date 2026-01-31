// src/commands/uptime.c
#include <stdint.h>
#include <kernel/printk.h>
#include <lib/commands.h>

// Basit uint -> decimal string cevirici
static void u32_to_dec(uint32_t value, char* buf) {
    char tmp[16];
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    // sondan basa dogru yaz
    while (value > 0 && i < (int)sizeof(tmp)) {
        uint32_t digit = value % 10;
        tmp[i++] = '0' + (char)digit;
        value /= 10;
    }

    // ters cevirip buf'a koy
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

// Fake uptime sayaci (saniye cinsinden)
static uint32_t fake_uptime_seconds = 0;

void cmd_uptime(int argc, char** argv) {
    (void)argc;
    (void)argv;

    char buf[16];

    // fake: her uptime cagrisinda 5 saniye arttiralim
    // ilk cagri -> 0
    // ikinci -> 5
    // ucuncu -> 10 ...
    u32_to_dec(fake_uptime_seconds, buf);

    printk("Uptime: ");
    printk(buf);
    printk(" seconds (fake)\n");

    fake_uptime_seconds += 5;
}

REGISTER_COMMAND(uptime, cmd_uptime, "Sistemin ne kadar suredir acik oldugunu gosterir");