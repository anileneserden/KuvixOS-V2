#include <kernel/drivers/input/mouse_ps2.h>
#include <arch/x86/io.h>
#include <stdint.h>

// Eğer yoksa, dosyanın en üstüne (include'lardan sonra) ekle:
int mouse_x = 0;
int mouse_y = 0;

static void ps2_wait_write(void) {
    while (inb(0x64) & 0x02) { }
}

static void ps2_wait_read(void) {
    while (!(inb(0x64) & 0x01)) { }
}

static int sign_extend_8(uint8_t v, int sign_bit_set) {
    if (sign_bit_set) return (int)((int32_t)(v | 0xFFFFFF00u));
    return (int)v;
}

void ps2_mouse_init(void)
{
    // enable aux device
    ps2_wait_write();
    outb(0x64, 0xA8);

    // read command byte
    ps2_wait_write();
    outb(0x64, 0x20);
    ps2_wait_read();
    uint8_t status = inb(0x60);

    // enable IRQ1 + IRQ12 bits (şimdilik polling olsa da dursun)
    status |= 0x02;
    status |= 0x04;

    ps2_wait_write();
    outb(0x64, 0x60);
    ps2_wait_write();
    outb(0x60, status);

    // set defaults
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, 0xF6);
    ps2_wait_read();
    (void)inb(0x60); // ACK

    // enable data reporting
    ps2_wait_write();
    outb(0x64, 0xD4);
    ps2_wait_write();
    outb(0x60, 0xF4);
    ps2_wait_read();
    (void)inb(0x60); // ACK
}

// 3-byte packet assembly
static uint8_t packet[3];
static int packet_index = 0;

// küçük event kuyruğu
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
        // dolu -> en eskiyi ez (istersen drop da edebilirsin)
        q_r = (q_r + 1) % MOUSE_QSIZE;
    }
    q[q_w].dx = dx;
    q[q_w].dy = dy;
    q[q_w].buttons = buttons;
    q_w = next;
}

void ps2_mouse_handle_byte(uint8_t data)
{
    // packet sync: first byte bit3 must be 1
    if (packet_index == 0) {
        if ((data & 0x08) == 0) return;   // sync değil
        if (data & 0xC0) return;          // overflow -> drop
    }

    packet[packet_index++] = data;
    if (packet_index < 3) return;

    packet_index = 0;

    uint8_t b = packet[0];

    // X sign bit: bit4, Y sign bit: bit5
    int dx = sign_extend_8(packet[1], (b & 0x10) != 0);
    int dy = sign_extend_8(packet[2], (b & 0x20) != 0);

    // PS/2: +Y = down. Ekranda +Y = down kullanıyorsan dy aynen kalsın.
    // Sen UI'da yukarı negatif istiyorsan:
    dy = -dy;

    uint8_t buttons = (uint8_t)(b & 0x07);

    q_push(dx, dy, buttons);
}

int ps2_mouse_pop(int* dx, int* dy, uint8_t* buttons)
{
    if (q_r == q_w) return 0;

    mouse_ev_t ev = q[q_r];
    q_r = (q_r + 1) % MOUSE_QSIZE;

    if (dx) *dx = ev.dx;
    if (dy) *dy = ev.dy;
    if (buttons) *buttons = ev.buttons;
    return 1;
}

void ps2_mouse_poll(void) {
    // 0x64 portu durum portudur. Bit 0 (0x01) veri olduğunu gösterir.
    while (inb(0x64) & 0x01) {
        uint8_t status = inb(0x64);
        uint8_t data = inb(0x60);
        
        // Eğer status bit 5 (0x20) set edilmişse bu veri MOUSE'a aittir
        if (status & 0x20) {
            ps2_mouse_handle_byte(data);
        }
    }
}