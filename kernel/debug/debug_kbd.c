#include <kernel/debug/debug_kbd.h>
#include <arch/x86/io.h>
#include <kernel/drivers/video/fb_console.h>
#include <stdint.h>

#define KBD_STATUS_PORT 0x64
#define KBD_DATA_PORT   0x60

static int kbd_has_data(void) { return (inb(KBD_STATUS_PORT) & 0x01) != 0; }
static uint8_t kbd_read_scancode(void) { return inb(KBD_DATA_PORT); }

// fb_console_write yoksa putc ile yazar
static void con_write(const char* s) {
#ifdef FB_CONSOLE_HAS_WRITE
    fb_console_write(s);
#else
    while (*s) fb_console_putc(*s++);
#endif
}

static char hex_digit(uint8_t v) { return (v < 10) ? ('0' + v) : ('A' + (v - 10)); }

static void u8_hex(char* out, uint8_t x) {
    out[0] = '0'; out[1] = 'x';
    out[2] = hex_digit((x >> 4) & 0xF);
    out[3] = hex_digit(x & 0xF);
    out[4] = 0;
}

static void clear_line(int row) {
    // satırı boşlukla doldur (ekran genişliğine göre)
    int cols = fb_console_cols();
    if (cols < 0) cols = 120; // fallback

    fb_console_set_cursor(0, row);
    for (int i = 0; i < cols; i++) fb_console_putc(' ');
    fb_console_set_cursor(0, row);
}

void debug_kbd_run(void) {
    fb_console_clear();
    
    // Başlık sabit (newline kullanmadan)
    clear_line(0);
    fb_console_set_cursor(0, 0);
    con_write("KBD DEBUG (tek satir) | ESC stop yok (simdilik)");
    fb_console_flush();

    int row = 2;
    int shift = 0;
    int e0 = 0;

    while (1) {
        if (!kbd_has_data()) continue;

        uint8_t sc = kbd_read_scancode();
        if (sc == 0xE0) { e0 = 1; continue; }

        int released = (sc & 0x80) != 0;
        uint8_t code = sc & 0x7F;

        // Shift state (E0 olmayan shift’ler)
        if (!e0) {
            if (!released && (code == 0x2A || code == 0x36)) shift = 1;
            if (released  && (code == 0x2A || code == 0x36)) shift = 0;
        }

        // Tek satır güncelle
        clear_line(row);

        char sc_hex[5], code_hex[5];
        u8_hex(sc_hex, sc);
        u8_hex(code_hex, code);

        con_write("SC=");
        con_write(sc_hex);
        con_write(" CODE=");
        con_write(code_hex);
        con_write(" ");

        con_write(released ? "UP " : "DN ");

        con_write(" E0=");
        con_write(e0 ? "1" : "0");
        con_write(" SHIFT=");
        con_write(shift ? "1" : "0");
        fb_console_flush();

        e0 = 0;
    }
}
