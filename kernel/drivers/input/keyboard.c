// kernel/drivers/input/keyboard.c
#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>   // layout API: kbd_set_layout(), kbd_get_current_layout()
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
static uint8_t head = 0;
static uint8_t tail = 0;

/* --------------------------------------------------
   MODIFIER STATE
-------------------------------------------------- */

static uint8_t g_shift = 0;
static uint8_t g_ctrl  = 0;
static uint8_t g_alt   = 0;
static uint8_t g_altgr = 0;
static uint8_t g_super = 0;

/* E0 prefix pending */
static uint8_t g_e0_pending = 0;

/* Duplicate make engelleme */
static uint8_t g_key_down[128];

/* --------------------------------------------------
   DEBUG EXPORTS
-------------------------------------------------- */

volatile uint8_t g_kbd_last_sc = 0;
volatile uint8_t g_kbd_last_is_break = 0;
volatile uint8_t g_kbd_last_e0 = 0;

/* --------------------------------------------------
   SCANCODE HELPERS
-------------------------------------------------- */

/* Normal Set1 */
static int is_shift_make(uint8_t sc)  { return (sc == 0x2A || sc == 0x36); }
static int is_shift_break(uint8_t sc) { return (sc == 0xAA || sc == 0xB6); }

static int is_ctrl_make(uint8_t sc)   { return (sc == 0x1D); }
static int is_ctrl_break(uint8_t sc)  { return (sc == 0x9D); }

static int is_alt_make(uint8_t sc)    { return (sc == 0x38); }
static int is_alt_break(uint8_t sc)   { return (sc == 0xB8); }

/* E0 prefix */
static int is_super_make_e0(uint8_t sc)   { return (sc == 0x5B || sc == 0x5C); }
static int is_super_break_e0(uint8_t sc)  { return (sc == 0xDB || sc == 0xDC); }

static int is_altgr_make_e0(uint8_t sc)   { return (sc == 0x38); }
static int is_altgr_break_e0(uint8_t sc)  { return (sc == 0xB8); }

/* --------------------------------------------------
   MODIFIER API
-------------------------------------------------- */

uint8_t kbd_mods(void) {
    return (g_shift ? 1 : 0)
         | (g_ctrl  ? 2 : 0)
         | (g_alt   ? 4 : 0);
}

int kbd_is_ctrl_pressed(void)   { return g_ctrl ? 1 : 0; }
int kbd_is_shift_pressed(void)  { return g_shift ? 1 : 0; }
int kbd_is_super_pressed(void)  { return g_super ? 1 : 0; }
int kbd_is_altgr_pressed(void)  { return g_altgr ? 1 : 0; }

/* --------------------------------------------------
   DEBUG API
-------------------------------------------------- */

static int g_kbd_debug_enabled = 0;

void kbd_debug_set(int enabled) {
    g_kbd_debug_enabled = enabled ? 1 : 0;
}

int kbd_debug_get(void) {
    return g_kbd_debug_enabled;
}

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
   OPTIONAL COMPAT API
-------------------------------------------------- */

void kbd_set_layout_trq(void) {
    kbd_set_layout("trq");
}

void kbd_handle_byte(uint8_t sc) {
    g_kbd_last_sc = sc;
    g_kbd_last_is_break = (sc & 0x80) ? 1 : 0;
    g_kbd_last_e0 = g_e0_pending ? 1 : 0;

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
-------------------------------------------------- */

uint16_t kbd_pop_event(void) {
    while (head != tail) {
        uint16_t ev = kbd_buffer[tail];
        tail = (uint8_t)((tail + 1) & 0xFF);

        uint8_t sc = (uint8_t)(ev & 0xFF);
        uint8_t is_e0 = ((ev & 0xFF00) == 0xE000);

        g_kbd_last_sc = sc;
        g_kbd_last_is_break = (sc & 0x80) ? 1 : 0;
        g_kbd_last_e0 = is_e0 ? 1 : 0;

        /* Modifier state update */
        if (!is_e0) {
            if (is_shift_make(sc))  { g_shift = 1; continue; }
            if (is_shift_break(sc)) { g_shift = 0; continue; }

            if (is_ctrl_make(sc))   { g_ctrl = 1; continue; }
            if (is_ctrl_break(sc))  { g_ctrl = 0; continue; }

            if (is_alt_make(sc))    { g_alt = 1; continue; }
            if (is_alt_break(sc))   { g_alt = 0; continue; }
        } else {
            if (is_altgr_make_e0(sc))  { g_altgr = 1; continue; }
            if (is_altgr_break_e0(sc)) { g_altgr = 0; continue; }

            if (is_super_make_e0(sc))  { g_super = 1; continue; }
            if (is_super_break_e0(sc)) { g_super = 0; continue; }
        }

        /* Duplicate make filter */
        {
            uint8_t code = (uint8_t)(sc & 0x7F);

            if (sc & 0x80) {
                if (code < 128) g_key_down[code] = 0;
                return ev;
            } else {
                if (code < 128) {
                    if (g_key_down[code]) {
#ifdef KBD_SERIAL_DEBUG
                        serial_write("[KBD] dup make ignored sc=0x");
                        serial_write_hex8(sc);
                        serial_write("\n");
#endif
                        continue;
                    }
                    g_key_down[code] = 1;
                }
                return ev;
            }
        }
    }

    return 0;
}

int kbd_has_character(void) {
    return (head != tail);
}

/* --------------------------------------------------
   CHAR / KEY TRANSLATION
   Returns:
     0                     -> no key
     ASCII char            -> normal key
     0xFF00 | KBD_*        -> special key
-------------------------------------------------- */

int kbd_get_char(void) {
    while (1) {
        uint16_t ev = kbd_pop_event();
        if (ev == 0) return 0;

        /* E0 special keys */
        if ((ev & 0xFF00) == 0xE000) {
            uint8_t sc = (uint8_t)(ev & 0xFF);

            /* break ignore */
            if (sc & 0x80)
                continue;

            if (sc == 0x48) return 0xFF00 | KBD_UP;
            if (sc == 0x50) return 0xFF00 | KBD_DOWN;
            if (sc == 0x4B) return 0xFF00 | KBD_LEFT;
            if (sc == 0x4D) return 0xFF00 | KBD_RIGHT;

            continue;
        }

        {
            uint8_t sc = (uint8_t)(ev & 0xFF);

            /* break ignore */
            if (sc & 0x80)
                continue;

            const kbd_layout_t* lay = kbd_get_current_layout();
            if (!lay) return 0;

            {
                uint8_t code = (uint8_t)(sc & 0x7F);
                const uint8_t* table = 0;

                if (g_altgr && lay->altgr)      table = lay->altgr;
                else if (g_shift && lay->shift) table = lay->shift;
                else                            table = lay->normal;

                if (!table) return 0;

                {
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

                    return (int)ch;
                }
            }
        }
    }
}

/* --------------------------------------------------
   Legacy helper
-------------------------------------------------- */

char kbd_scancode_to_ascii(uint8_t sc) {
    if (sc & 0x80) return 0;

    {
        const kbd_layout_t* lay = kbd_get_current_layout();
        if (!lay) return 0;

        {
            uint8_t code = (uint8_t)(sc & 0x7F);
            const uint8_t* table = 0;

            if (g_altgr && lay->altgr)      table = lay->altgr;
            else if (g_shift && lay->shift) table = lay->shift;
            else                            table = lay->normal;

            if (!table) return 0;

            return (char)table[code];
        }
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

        kbd_handle_byte(sc);
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
            kbd_handle_byte(data);
        }
    }

    outb(0x20, 0x20);
}