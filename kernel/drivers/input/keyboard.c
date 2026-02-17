#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h>
#include <lib/string.h>
#include <stdint.h>

// ✅ SERIAL DEBUG
#include <kernel/serial.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

/* Basit bir event kuyruğu */
static uint16_t kbd_buffer[256];
static uint8_t head = 0;
static uint8_t tail = 0;

// --- Shift state ---
static uint8_t g_shift = 0;

// Set1 scancodes:
// LSHIFT down 0x2A, up 0xAA
// RSHIFT down 0x36, up 0xB6
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
    // TEMİZLİK: Sadece klavye verilerini temizle, fare verilerine dokunma
    uint8_t status;
    while ((status = inb(KBD_STATUS_PORT)) & 0x01) {
        (void)inb(KBD_DATA_PORT);
    }

    outb(KBD_STATUS_PORT, 0xAE); // Klavyeyi aktif et
    current_layout = &layout_trq;
    g_shift = 0;

#ifdef KBD_SERIAL_DEBUG
    serial_write("[KBD] init done\n");
#endif
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0;
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 256;
    return code;
}

char kbd_get_char(void) {
    while (1) {
        uint16_t scancode = kbd_pop_event();
        if (scancode == 0) return 0;

        uint8_t sc = (uint8_t)scancode;

        // Shift state güncelle (Set1 make/break)
        if (is_shift_make(sc)) { g_shift = 1; continue; }
        if (is_shift_break(sc)) { g_shift = 0; continue; }

        // Release bit set ise karakter üretme
        if (sc & 0x80) continue;

        if (!current_layout) return 0;

        uint8_t code = (uint8_t)(sc & 0x7F);
        const uint8_t* table = g_shift ? current_layout->shift : current_layout->normal;
        if (!table) return 0;

        uint8_t ch = table[code];
        if (ch == 0) continue;

#ifdef KBD_SERIAL_DEBUG
        serial_write("[KBD] sc=0x");
        serial_write_hex8(sc);
        serial_write(" code=0x");
        serial_write_hex8(code);
        serial_write(" shift=");
        serial_putc(g_shift ? '1' : '0');
        serial_write(" -> ch=0x");
        serial_write_hex8(ch);
        serial_write(" '");
        serial_putc((ch >= 32 && ch <= 126) ? (char)ch : '.');
        serial_write("'\n");
#endif

        return (char)ch;
    }
}

int kbd_has_character(void) {
    return (head != tail);
}

/**
 * @brief Donanım portunu kontrol eder. 
 * Çakışmayı önlemek için 5. biti kontrol eder.
 */
void kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    // Veri var mı ve mouse verisi değil mi?
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(KBD_DATA_PORT);

#ifdef KBD_SERIAL_DEBUG
        serial_write("[KBD] raw=0x");
        serial_write_hex8(sc);
        serial_write("\n");
#endif

        kbd_push_scan_code(sc);
    }
}

// Assembly'deki "call kbd_handler" burayı çalıştıracak
void kbd_handler(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    if (status & 0x01) {
        uint8_t data = inb(KBD_DATA_PORT);

        if (!(status & 0x20)) {
#ifdef KBD_SERIAL_DEBUG
            serial_write("[KBD] irq raw=0x");
            serial_write_hex8(data);
            serial_write("\n");
#endif
            kbd_push_scan_code(data);
        }
    }

    outb(0x20, 0x20);
}