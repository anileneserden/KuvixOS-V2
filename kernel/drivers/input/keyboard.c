#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>              // layout API
#include <arch/x86/io.h>
#include <lib/string.h>
#include <stdint.h>

#ifdef KBD_SERIAL_DEBUG
#include <kernel/serial.h>
#endif

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

/* --------------------------------------------------
   RAW SCANCODE BUFFER
-------------------------------------------------- */

static uint16_t kbd_buffer[256];
static uint8_t head = 0;
static uint8_t tail = 0;

/* --------------------------------------------------
   MODIFIER STATE
-------------------------------------------------- */

static uint8_t g_shift = 0;
static uint8_t g_ctrl  = 0;
static uint8_t g_alt   = 0;
static uint8_t g_key_down[128];

// Set1 scancodes
static int is_shift_make(uint8_t sc)  { return (sc == 0x2A || sc == 0x36); }
static int is_shift_break(uint8_t sc) { return (sc == 0xAA || sc == 0xB6); }

static int is_ctrl_make(uint8_t sc)   { return (sc == 0x1D); }
static int is_ctrl_break(uint8_t sc)  { return (sc == 0x9D); }

static int is_alt_make(uint8_t sc)    { return (sc == 0x38); }
static int is_alt_break(uint8_t sc)   { return (sc == 0xB8); }

// bit0=shift bit1=ctrl bit2=alt
uint8_t kbd_mods(void) {
    return (g_shift ? 1 : 0)
         | (g_ctrl  ? 2 : 0)
         | (g_alt   ? 4 : 0);
}

/* --------------------------------------------------
   BUFFER PUSH
-------------------------------------------------- */

static void kbd_push_scan_code(uint8_t scancode) {
    uint8_t next = (head + 1) & 0xFF;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

/* --------------------------------------------------
   INIT
-------------------------------------------------- */

void kbd_init(void) {
    uint8_t status;
    while ((status = inb(KBD_STATUS_PORT)) & 0x01) {
        (void)inb(KBD_DATA_PORT);
    }

    outb(KBD_STATUS_PORT, 0xAE);

    kbd_set_layout("trq");

    g_shift = 0;
    g_ctrl  = 0;
    g_alt   = 0;
    memset(g_key_down, 0, sizeof(g_key_down));

#ifdef KBD_SERIAL_DEBUG
    serial_write("[KBD] init done\n");
#endif
}

/* --------------------------------------------------
   POP RAW EVENT
-------------------------------------------------- */

uint16_t kbd_pop_event(void) {
    while (head != tail) {

        uint16_t raw = kbd_buffer[tail];
        tail = (tail + 1) & 0xFF;

        uint8_t sc = (uint8_t)raw;

        // --- Modifier state update (make/break) ---
        if (is_shift_make(sc))  { g_shift = 1; continue; }
        if (is_shift_break(sc)) { g_shift = 0; continue; }

        if (is_ctrl_make(sc))   { g_ctrl = 1;  continue; }
        if (is_ctrl_break(sc))  { g_ctrl = 0;  continue; }

        if (is_alt_make(sc))    { g_alt = 1;   continue; }
        if (is_alt_break(sc))   { g_alt = 0;   continue; }

        // --- Key down filter (duplicate make protection) ---
        uint8_t code = (uint8_t)(sc & 0x7F);

        if (sc & 0x80) {
            // break -> key up
            if (code < 128) g_key_down[code] = 0;
            return raw; // break event'i isteyen kullanabilir
        } else {
            // make -> eğer zaten down ise yut (duplicate make)
            if (code < 128) {
                if (g_key_down[code]) {
#ifdef KBD_SERIAL_DEBUG
                    serial_write("[KBD] dup make ignored sc=0x");
                    serial_write_hex8(sc);
                    serial_write("\n");
#endif
                    continue; // 🔥 asıl fix burada
                }
                g_key_down[code] = 1;
            }
            return raw; // make event
        }
    }

    return 0;
}

int kbd_has_character(void) {
    return (head != tail);
}

/* --------------------------------------------------
   CHAR TRANSLATION
-------------------------------------------------- */

char kbd_get_char(void) {

    while (1) {

        uint16_t ev = kbd_pop_event();
        if (ev == 0)
            return 0;

        uint8_t sc = (uint8_t)ev;

        // Break event -> ignore
        if (sc & 0x80)
            continue;

        const kbd_layout_t* lay = kbd_get_current_layout();
        if (!lay)
            return 0;

        uint8_t code = sc & 0x7F;
        const uint8_t* table = g_shift ? lay->shift : lay->normal;
        if (!table)
            return 0;

        uint8_t ch = table[code];
        if (ch == 0)
            continue;

#ifdef KBD_SERIAL_DEBUG
        serial_write("[KBD] sc=0x");
        serial_write_hex8(sc);
        serial_write(" layout=");
        serial_write(lay->name);
        serial_write(" mods=");
        serial_write_hex8(kbd_mods());
        serial_write(" ch=0x");
        serial_write_hex8(ch);
        serial_write("\n");
#endif

        return (char)ch;
    }
}

/* --------------------------------------------------
   POLLING
-------------------------------------------------- */

void kbd_poll(void) {

    uint8_t status = inb(KBD_STATUS_PORT);

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

/* --------------------------------------------------
   IRQ HANDLER
-------------------------------------------------- */

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

    outb(0x20, 0x20); // PIC EOI
}