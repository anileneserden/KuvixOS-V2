#include <kernel/drivers/input/mouse_ps2.h>
#include <arch/x86/io.h>
#include <stdint.h>
#include <ui/wm.h>

// Telemetri için (debug_screen'den okunacak)
volatile int32_t g_mouse_last_dx = 0;
volatile int32_t g_mouse_last_dy = 0;
volatile uint32_t g_mouse_irq_count = 0;

// Eski global koordinatlar (varmış, bıraktım)
int mouse_x = 400;
int mouse_y = 300;

static int ps2_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(inb(0x64) & 0x02)) return 1;
    }
    return 0;
}

static int ps2_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) return 1;
    }
    return 0;
}

static int sign_extend_8(uint8_t v, int sign_bit_set) {
    if (sign_bit_set) return (int)((int32_t)(v | 0xFFFFFF00u));
    return (int)v;
}

static void ps2_mouse_write(uint8_t data) {
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, data);
}

void ps2_mouse_init(void) {
    // A. Flush
    for (int i = 0; i < 32; i++) {
        if (inb(0x64) & 0x01) (void)inb(0x60);
    }

    // B. AUX disable
    ps2_wait_write();
    outb(0x64, 0xA7);

    // C. AUX enable
    ps2_wait_write();
    outb(0x64, 0xA8);

    // D. Command byte ayarla
    ps2_wait_write();
    outb(0x64, 0x20);
    ps2_wait_read();
    uint8_t status = inb(0x60);

    status |= 0x02;   // IRQ12 enable
    status &= ~0x20;  // Mouse disable bitini kaldır

    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, status);

    // E. defaults
    ps2_mouse_write(0xF6);
    ps2_wait_read();
    (void)inb(0x60); // ACK

    // F. enable reporting
    ps2_mouse_write(0xF4);
    ps2_wait_read();
    (void)inb(0x60); // ACK

    // G. Flush
    while (inb(0x64) & 0x01) (void)inb(0x60);
}

// 3-byte packet assembly
static uint8_t packet[3];
static int packet_index = 0;

typedef struct {
    int dx, dy;
    uint8_t buttons;
} mouse_ev_t;

#define MOUSE_QSIZE 32
static mouse_ev_t q[MOUSE_QSIZE];
static int q_r = 0, q_w = 0;

static void q_push(int dx, int dy, uint8_t buttons) {
    int next = (q_w + 1) % MOUSE_QSIZE;
    if (next == q_r) {
        // dolu -> en eskiyi ez
        q_r = (q_r + 1) % MOUSE_QSIZE;
    }
    q[q_w].dx = dx;
    q[q_w].dy = dy;
    q[q_w].buttons = buttons;
    q_w = next;
}

void ps2_mouse_handle_byte(uint8_t data) {
    if (packet_index == 0) {
        // sync bit (bit3) şart
        if ((data & 0x08) == 0) return;
        // overflow bayrakları set ise discard
        if (data & 0xC0) return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) return;

    packet_index = 0;

    uint8_t b = packet[0];
    int dx = sign_extend_8(packet[1], (b & 0x10) != 0);
    int dy = sign_extend_8(packet[2], (b & 0x20) != 0);
    dy = -dy;

    // Telemetri için son dx/dy
    g_mouse_last_dx = dx;
    g_mouse_last_dy = dy;

    q_push(dx, dy, b & 0x07);
}

int ps2_mouse_pop(int* dx, int* dy, uint8_t* buttons) {
    if (q_r == q_w) return 0;

    mouse_ev_t ev = q[q_r];
    q_r = (q_r + 1) % MOUSE_QSIZE;

    if (dx) *dx = ev.dx;
    if (dy) *dy = ev.dy;
    if (buttons) *buttons = ev.buttons;
    return 1;
}

void ps2_mouse_update(void) {
    int dx, dy;
    uint8_t buttons;
    static uint8_t last_buttons = 0;

    while (ps2_mouse_pop(&dx, &dy, &buttons)) {
        mouse_x += dx;
        mouse_y += dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1023) mouse_x = 1023;
        if (mouse_y > 767)  mouse_y = 767;

#ifndef DEBUG_SELFTEST
        wm_handle_mouse_move(mouse_x, mouse_y);

        uint8_t pressed  = buttons & ~last_buttons;
        uint8_t released = last_buttons & ~buttons;

        if (pressed || released) {
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, buttons);
        }
#endif

        last_buttons = buttons;
    }
}

// (Opsiyonel) polling’i test modunda kullanacaksan IRQ'yu kapat.
// Aksi halde polling + IRQ beraber çalışmasın.
void ps2_mouse_poll(void) {
    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        uint8_t data   = inb(0x60);
        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        } else {
            // klavye byte'ı -> drop (buffer boşalsın yeter)
        }
    }
}

// IRQ12 handler: EN KRİTİK FIX -> 0x60'ı her zaman oku (buffer boşalsın)
void mouse_handler(void) {
    g_mouse_irq_count++;

    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        uint8_t data   = inb(0x60); // HER ZAMAN oku

        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        } else {
            // klavye byte'ı -> drop (istersen kbd kuyruğuna atarsın)
        }
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
    g_mouse_irq_count++;

    for (int i = 0; i < 32; i++)
    {
        uint8_t status = inb(0x64);

        if ((status &0x01) == 0)
            break;
        
        uint8_t data = inb(0x60);

        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        } else {

        }
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}