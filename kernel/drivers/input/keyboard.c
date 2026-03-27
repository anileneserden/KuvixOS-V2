#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h>
#include <lib/string.h>
#include <stdint.h>
#include <kernel/serial.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

static uint16_t kbd_buffer[256];
static uint8_t head = 0;
static uint8_t tail = 0;

static uint8_t g_shift = 0;
static uint8_t g_e0 = 0;

static int is_shift_make(uint8_t sc) { return (sc == 0x2A || sc == 0x36); }
static int is_shift_break(uint8_t sc){ return (sc == 0xAA || sc == 0xB6); }

extern kbd_layout_t layout_trq;
extern kbd_layout_t layout_us;
static kbd_layout_t* current_layout = &layout_us;

void kbd_push_scan_code(uint8_t scancode) {
    uint8_t next = (head + 1) % 256;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

void kbd_init(void) {
    uint8_t status;
    while ((status = inb(KBD_STATUS_PORT)) & 0x01) {
        (void)inb(KBD_DATA_PORT);
    }

    outb(KBD_STATUS_PORT, 0xAE);
    current_layout = &layout_trq;
    g_shift = 0;
    g_e0 = 0;
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0;
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 256;
    return code;
}

int kbd_get_char(void) {
    while (1) {
        uint16_t scancode = kbd_pop_event();
        if (scancode == 0) return 0;

        uint8_t sc = (uint8_t)scancode;

        // 🔥 E0 prefix
        if (sc == 0xE0) {
            g_e0 = 1;
            continue;
        }

        // Shift
        if (is_shift_make(sc)) { g_shift = 1; continue; }
        if (is_shift_break(sc)){ g_shift = 0; continue; }

        // release ignore
        if (sc & 0x80) continue;

        // 🔥 SPECIAL KEYS
        if (g_e0) {
            g_e0 = 0;

            if (sc == 0x48) return 0xFF00 | KBD_UP;
            if (sc == 0x50) return 0xFF00 | KBD_DOWN;
            if (sc == 0x4B) return 0xFF00 | KBD_LEFT;
            if (sc == 0x4D) return 0xFF00 | KBD_RIGHT;

            continue;
        }

        if (!current_layout) return 0;

        uint8_t code = sc & 0x7F;
        const uint8_t* table = g_shift ? current_layout->shift : current_layout->normal;
        if (!table) return 0;

        uint8_t ch = table[code];
        if (ch == 0) continue;

        return (int)ch;
    }
}

int kbd_has_character(void) {
    return (head != tail);
}

void kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(KBD_DATA_PORT);
        kbd_push_scan_code(sc);
    }
}

void kbd_handler(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    if (status & 0x01) {
        uint8_t data = inb(KBD_DATA_PORT);

        if (!(status & 0x20)) {
            kbd_push_scan_code(data);
        }
    }

    outb(0x20, 0x20);
}