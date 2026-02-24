// kernel/drivers/input/keyboard.c
#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h>
#include <lib/string.h>
#include <stdint.h>

#ifdef KBD_SERIAL_DEBUG
#include <kernel/serial.h>
#endif

#define KBD_DATA_PORT   0x60
#define KBD_STATUS_PORT 0x64

/* --------------------------------------------------
   RAW SCANCODE BUFFER (16-bit)
-------------------------------------------------- */

static uint16_t kbd_buffer[256];
static uint8_t  head = 0;
static uint8_t  tail = 0;

/* --------------------------------------------------
   MODIFIER STATE
-------------------------------------------------- */

static uint8_t g_shift = 0;
static uint8_t g_ctrl  = 0;
static uint8_t g_alt   = 0;     // Left Alt (0x38)
static uint8_t g_altgr = 0;     // Right Alt (E0 38)
static uint8_t g_super = 0;     // Win key (E0 5B/5C)

static uint8_t g_e0_pending = 0;    // E0 prefix pending
static uint8_t g_key_down[128];     // duplicate make filter

// Set1 (normal)
static int is_shift_make(uint8_t sc)  { return (sc == 0x2A || sc == 0x36); }
static int is_shift_break(uint8_t sc) { return (sc == 0xAA || sc == 0xB6); }

static int is_ctrl_make(uint8_t sc)   { return (sc == 0x1D); }
static int is_ctrl_break(uint8_t sc)  { return (sc == 0x9D); }

static int is_alt_make(uint8_t sc)    { return (sc == 0x38); }
static int is_alt_break(uint8_t sc)   { return (sc == 0xB8); }

// E0 prefix: Super/Win
// LeftWin make: E0 5B  break: E0 DB
// RightWin make: E0 5C break: E0 DC
static int is_super_make_e0(uint8_t sc)  { return (sc == 0x5B || sc == 0x5C); }
static int is_super_break_e0(uint8_t sc) { return (sc == 0xDB || sc == 0xDC); }

// E0 prefix: Right Alt (AltGr) is E0 38 / E0 B8
static int is_altgr_make_e0(uint8_t sc)  { return (sc == 0x38); }
static int is_altgr_break_e0(uint8_t sc) { return (sc == 0xB8); }

// bit0=shift bit1=ctrl bit2=alt bit3=altgr bit4=super (istersen)
uint8_t kbd_mods(void) {
    return (g_shift ? 1 : 0)
         | (g_ctrl  ? 2 : 0)
         | (g_alt   ? 4 : 0)
         | (g_altgr ? 8 : 0)
         | (g_super ? 16 : 0);
}

int kbd_is_ctrl_pressed(void)  { return g_ctrl  ? 1 : 0; }
int kbd_is_shift_pressed(void) { return g_shift ? 1 : 0; }
int kbd_is_alt_pressed(void)   { return g_alt   ? 1 : 0; }
int kbd_is_altgr_pressed(void) { return g_altgr ? 1 : 0; }
int kbd_is_super_pressed(void) { return g_super ? 1 : 0; }

/* --------------------------------------------------
   BUFFER PUSH (16-bit)
-------------------------------------------------- */

static void kbd_push16(uint16_t ev) {
    uint8_t next = (uint8_t)((head + 1) & 0xFF);
    if (next != tail) {
        kbd_buffer[head] = ev;
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

    // Enable keyboard interface (PS/2 controller)
    outb(KBD_STATUS_PORT, 0xAE);

    // Default layout
    kbd_set_layout("trq");

    g_shift = 0;
    g_ctrl  = 0;
    g_alt   = 0;
    g_altgr = 0;
    g_super = 0;
    g_e0_pending = 0;

    memset(g_key_down, 0, sizeof(g_key_down));

#ifdef KBD_SERIAL_DEBUG
    serial_write("[KBD] init done\n");
#endif
}

/* --------------------------------------------------
   POP RAW EVENT
   - Returns one raw event (8-bit scancode or 0xE0xx)
   - Updates modifier states but DOES NOT swallow events
     (so inputtest can see Shift/Ctrl/Alt etc.)
-------------------------------------------------- */

uint16_t kbd_pop_event(void) {
    while (head != tail) {
        uint16_t ev = kbd_buffer[tail];
        tail = (uint8_t)((tail + 1) & 0xFF);

        uint8_t sc   = (uint8_t)(ev & 0xFF);
        uint8_t is_e0 = ((ev & 0xFF00) == 0xE000);

        // --- modifier update (do NOT continue; return event) ---
        if (!is_e0) {
            if (is_shift_make(sc))  { g_shift = 1; return ev; }
            if (is_shift_break(sc)) { g_shift = 0; return ev; }

            if (is_ctrl_make(sc))   { g_ctrl = 1; return ev; }
            if (is_ctrl_break(sc))  { g_ctrl = 0; return ev; }

            if (is_alt_make(sc))    { g_alt  = 1; return ev; }
            if (is_alt_break(sc))   { g_alt  = 0; return ev; }
        } else {
            if (is_altgr_make_e0(sc))  { g_altgr = 1; return ev; }
            if (is_altgr_break_e0(sc)) { g_altgr = 0; return ev; }

            if (is_super_make_e0(sc))  { g_super = 1; return ev; }
            if (is_super_break_e0(sc)) { g_super = 0; return ev; }
        }

        // --- duplicate make filter (optional) ---
        // IMPORTANT: Only apply this filter to non-modifier keys,
        // otherwise Shift/Ctrl/Alt "make" might be suppressed in debug.
        // We'll keep it simple: filter only non-E0 and not shift/ctrl/alt codes.
        if (!is_e0) {
            uint8_t code = (uint8_t)(sc & 0x7F);

            // break
            if (sc & 0x80) {
                if (code < 128) g_key_down[code] = 0;
                return ev;
            }

            // make
            if (code < 128) {
                // skip filtering for known modifiers
                if (!is_shift_make(sc) && !is_ctrl_make(sc) && !is_alt_make(sc)) {
                    if (g_key_down[code]) {
#ifdef KBD_SERIAL_DEBUG
                        serial_write("[KBD] dup make ignored sc=0x");
                        serial_write_hex8(sc);
                        serial_write("\n");
#endif
                        continue; // ignore duplicate
                    }
                    g_key_down[code] = 1;
                }
            }
            return ev;
        }

        // E0 event: just return it (super/altgr already updated above when applicable)
        return ev;
    }

    return 0;
}

int kbd_has_character(void) {
    return (head != tail);
}

/* --------------------------------------------------
   CHAR TRANSLATION (layout)
-------------------------------------------------- */

char kbd_get_char(void) {
    while (1) {
        uint16_t ev = kbd_pop_event();
        if (ev == 0) return 0;

        // E0 events don't produce characters
        if ((ev & 0xFF00) == 0xE000)
            continue;

        uint8_t sc = (uint8_t)(ev & 0xFF);

        // break -> ignore
        if (sc & 0x80)
            continue;

        const kbd_layout_t* lay = kbd_get_current_layout();
        if (!lay) return 0;

        uint8_t code = (uint8_t)(sc & 0x7F);

        const uint8_t* table = 0;
        if (g_altgr && lay->altgr) table = lay->altgr;
        else if (g_shift)         table = lay->shift;
        else                      table = lay->normal;

        if (!table) return 0;

        uint8_t ch = table[code];
        if (ch == 0)
            continue;

#ifdef KBD_SERIAL_DEBUG
        serial_write("[KBD] sc=0x");
        serial_write_hex8(sc);
        serial_write(" mods=0x");
        serial_write_hex8(kbd_mods());
        serial_write(" ch=0x");
        serial_write_hex8(ch);
        serial_write("\n");
#endif

        return (char)ch;
    }
}

// Compatibility helper: ONLY for normal make codes (no E0)
char kbd_scancode_to_ascii(uint8_t sc) {
    if (sc & 0x80) return 0; // break -> no char

    const kbd_layout_t* lay = kbd_get_current_layout();
    if (!lay) return 0;

    uint8_t code = (uint8_t)(sc & 0x7F);

    const uint8_t* table = 0;
    if (g_altgr && lay->altgr) table = lay->altgr;
    else if (g_shift)         table = lay->shift;
    else                      table = lay->normal;

    if (!table) return 0;

    return (char)table[code];
}

/* --------------------------------------------------
   POLLING (optional)
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

        // E0 prefix combine
        if (sc == 0xE0) {
            g_e0_pending = 1;
            return;
        }

        if (g_e0_pending) {
            g_e0_pending = 0;
            kbd_push16((uint16_t)(0xE000 | sc));
            return;
        }

        kbd_push16((uint16_t)sc);
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

            // E0 prefix combine
            if (data == 0xE0) {
                g_e0_pending = 1;
                outb(0x20, 0x20);
                return;
            }

            if (g_e0_pending) {
                g_e0_pending = 0;
                kbd_push16((uint16_t)(0xE000 | data));
                outb(0x20, 0x20);
                return;
            }

            kbd_push16((uint16_t)data);
        }
    }

    outb(0x20, 0x20); // PIC EOI
}