// kernel/drivers/input/mouse_ps2.c
#include <kernel/drivers/input/mouse_ps2.h>
#include <arch/x86/io.h>
#include <stdint.h>
#include <ui/wm.h>

// Telemetri (debug_screen'den okunacak)
volatile int32_t g_mouse_last_dx = 0;
volatile int32_t g_mouse_last_dy = 0;
volatile int32_t g_mouse_last_wheel = 0;
volatile uint32_t g_mouse_irq_count = 0;

// Eski global koordinatlar
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

static uint8_t ps2_mouse_read_byte(void) {
    ps2_wait_read();
    return inb(0x60);
}

static uint8_t ps2_mouse_read_ack(void) {
    // Mouse komutlarına ACK genelde 0xFA döner.
    // Çok sıkı kontrol etmek istemezsen direkt oku geç.
    return ps2_mouse_read_byte();
}

static void ps2_mouse_set_sample_rate(uint8_t rate) {
    ps2_mouse_write(0xF3);      // Set Sample Rate
    (void)ps2_mouse_read_ack(); // ACK
    ps2_mouse_write(rate);
    (void)ps2_mouse_read_ack(); // ACK
}

static uint8_t ps2_mouse_get_device_id(void) {
    ps2_mouse_write(0xF2);      // Get Device ID
    (void)ps2_mouse_read_ack(); // ACK
    return ps2_mouse_read_byte(); // ID
}

// ------------------------------------------------------------
// Packet assembly (3-byte default, 4-byte if wheel enabled)
// ------------------------------------------------------------
static uint8_t packet[4];
static int packet_index = 0;
static int packet_size  = 3; // 3 = standart, 4 = wheel (IntelliMouse)

// Event queue
typedef struct {
    int dx, dy;
    int wheel;          // 0 normally, +/- on wheel
    uint8_t buttons;    // bit0 L, bit1 R, bit2 M
} mouse_ev_t;

#define MOUSE_QSIZE 32
static mouse_ev_t q[MOUSE_QSIZE];
static int q_r = 0, q_w = 0;

static void q_push(int dx, int dy, int wheel, uint8_t buttons) {
    int next = (q_w + 1) % MOUSE_QSIZE;
    if (next == q_r) {
        // dolu -> en eskiyi ez
        q_r = (q_r + 1) % MOUSE_QSIZE;
    }
    q[q_w].dx = dx;
    q[q_w].dy = dy;
    q[q_w].wheel = wheel;
    q[q_w].buttons = buttons;
    q_w = next;
}

int ps2_mouse_pop(int* dx, int* dy, int* wheel, uint8_t* buttons) {
    if (q_r == q_w) return 0;

    mouse_ev_t ev = q[q_r];
    q_r = (q_r + 1) % MOUSE_QSIZE;

    if (dx) *dx = ev.dx;
    if (dy) *dy = ev.dy;
    if (wheel) *wheel = ev.wheel;
    if (buttons) *buttons = ev.buttons;
    return 1;
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
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
    (void)ps2_mouse_read_ack(); // ACK

    // --- Wheel enable (IntelliMouse) ---
    // Set sample rate sequence: 200, 100, 80 then Get ID
    ps2_mouse_set_sample_rate(200);
    ps2_mouse_set_sample_rate(100);
    ps2_mouse_set_sample_rate(80);

    uint8_t id = ps2_mouse_get_device_id();
    packet_size = (id == 3) ? 4 : 3;

    // F. enable reporting
    ps2_mouse_write(0xF4);
    (void)ps2_mouse_read_ack(); // ACK

    // G. Flush
    while (inb(0x64) & 0x01) (void)inb(0x60);

    // reset assembly
    packet_index = 0;
    g_mouse_last_wheel = 0;
}

// ------------------------------------------------------------
// Byte handler (called from IRQ / poll)
// ------------------------------------------------------------
void ps2_mouse_handle_byte(uint8_t data) {
    if (packet_index == 0) {
        // sync bit (bit3) şart
        if ((data & 0x08) == 0) return;
        // overflow bayrakları set ise discard
        if (data & 0xC0) return;
    }

    packet[packet_index++] = data;
    if (packet_index < packet_size) return;

    packet_index = 0;

    uint8_t b = packet[0];

    int dx = sign_extend_8(packet[1], (b & 0x10) != 0);
    int dy = sign_extend_8(packet[2], (b & 0x20) != 0);
    dy = -dy;

    int wheel = 0;
    if (packet_size == 4) {
        // IntelliMouse / Explorer formatında wheel genelde low-nibble signed (4-bit)
        int8_t w4 = (int8_t)(packet[3] & 0x0F);
        if (w4 & 0x08) w4 |= (int8_t)0xF0;  // sign-extend 4-bit -> 8-bit

        wheel = (int)w4;

        // Explorer (ID=4) extra buttons (bit4/bit5) de burada durur
        // İstersen buttons'a ekleyebilirsin:
        // uint8_t extra = (packet[3] & 0x30) >> 1; // örnek mapping (istersen düzenleriz)
        // buttons |= extra;

        g_mouse_last_wheel = wheel;
    } else {
        g_mouse_last_wheel = 0;
    }

    // Telemetri
    g_mouse_last_dx = dx;
    g_mouse_last_dy = dy;

    // buttons: low 3 bits
    q_push(dx, dy, wheel, (uint8_t)(b & 0x07));
}

// ------------------------------------------------------------
// Update -> WM
// ------------------------------------------------------------
void ps2_mouse_update(void) {
    int dx, dy, wheel;
    uint8_t buttons;
    static uint8_t last_buttons = 0;

    while (ps2_mouse_pop(&dx, &dy, &wheel, &buttons)) {
        mouse_x += dx;
        mouse_y += dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > 1023) mouse_x = 1023;
        if (mouse_y > 767)  mouse_y = 767;

#ifndef DEBUG_SELFTEST
        wm_handle_mouse_move(mouse_x, mouse_y);

        uint8_t pressed  = (uint8_t)(buttons & ~last_buttons);
        uint8_t released = (uint8_t)(last_buttons & ~buttons);

        if (pressed || released) {
            wm_handle_mouse(mouse_x, mouse_y, pressed, released, buttons);
        }

        // WHEEL event:
        // WM tarafına wheel route etmek için 2 seçeneğin var:
        // 1) wm_handle_mouse_wheel(...) diye yeni fonksiyon ekle
        // 2) wm_handle_mouse(...) imzasını genişlet (e1/e2 gibi)
        //
        // Şimdilik sadece telemetri olarak duruyor.
        // Eğer WM'e eklemek istersen örnek:
        if (wheel != 0) wm_handle_mouse_wheel(mouse_x, mouse_y, wheel, buttons);
#endif

        last_buttons = buttons;
    }
}

// ------------------------------------------------------------
// Poll (test mode)
// ------------------------------------------------------------
void ps2_mouse_poll(void) {
    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        uint8_t data   = inb(0x60);
        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        } else {
            // klavye byte'ı -> drop
        }
    }
}

// ------------------------------------------------------------
// IRQ12 handler
// ------------------------------------------------------------
void mouse_handler(void) {
    g_mouse_irq_count++;

    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        uint8_t data   = inb(0x60);

        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        } else {
            // klavye byte'ı -> drop
        }
    }

    // EOI
    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}