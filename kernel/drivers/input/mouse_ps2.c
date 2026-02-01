#include <kernel/drivers/input/mouse_ps2.h>
#include <arch/x86/io.h>
#include <stdint.h>

// Eğer yoksa, dosyanın en üstüne (include'lardan sonra) ekle:
int mouse_x = 400; // Ekran genişliğine göre (örn: 1024 / 2)
int mouse_y = 300; // Ekran yüksekliğine göre (örn: 768 / 2)

static int ps2_wait_write(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (!(inb(0x64) & 0x02)) return 1; // Yazmaya hazır
    }
    return 0; // Zaman aşımı
}

static int ps2_wait_read(void) {
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) return 1; // Okumaya hazır
    }
    return 0; // Zaman aşımı
}

static int sign_extend_8(uint8_t v, int sign_bit_set) {
    if (sign_bit_set) return (int)((int32_t)(v | 0xFFFFFF00u));
    return (int)v;
}

static void ps2_mouse_write(uint8_t data) {
    // Fareye veri göndermek için önce 0x64 portuna 0xD4 yazılır
    // Bu, klavye denetleyicisine "sonraki veriyi fareye (AUX) ilet" der.
    ps2_wait_write();
    outb(0x64, 0xD4);
    
    // Ardından asıl veri 0x60 portuna yazılır
    ps2_wait_write();
    outb(0x60, data);
}

void ps2_mouse_init(void) {
    // A. Önce her şeyi bir temizle (Flush)
    for(int i = 0; i < 10; i++) {
        if(inb(0x64) & 0x01) inb(0x60);
    }

    // B. Fareyi Devre Dışı Bırak (Güvenli başlangıç için)
    ps2_wait_write();
    outb(0x64, 0xA7); 

    // C. Fareyi Tekrar Aktif Et
    ps2_wait_write();
    outb(0x64, 0xA8);

    // D. Komut Byte'ını Ayarla
    ps2_wait_write();
    outb(0x64, 0x20); // Oku
    ps2_wait_read();
    uint8_t status = inb(0x60);
    status |= 0x02;   // IRQ12 aktif
    status &= ~0x20;  // Mouse disable bitini kaldır

    ps2_wait_write();
    outb(0x64, 0x60); // Yaz
    ps2_wait_write();
    outb(0x60, status);

    // E. Fareye "Varsayılanları Yükle" de (ACK beklemeli)
    ps2_mouse_write(0xF6);
    ps2_wait_read();
    inb(0x60); // ACK oku

    // F. Veri Raporlamayı Aç
    ps2_mouse_write(0xF4);
    ps2_wait_read();
    inb(0x60); // ACK oku
    
    // G. Son bir temizlik
    while(inb(0x64) & 0x01) inb(0x60);
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

// drivers/input/mouse_ps2.c

void ps2_mouse_handle_byte(uint8_t data)
{
    if (packet_index == 0) {
        if ((data & 0x08) == 0) return;
        if (data & 0xC0) return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) return;

    packet_index = 0;

    uint8_t b = packet[0];
    int dx = sign_extend_8(packet[1], (b & 0x10) != 0);
    int dy = sign_extend_8(packet[2], (b & 0x20) != 0);
    dy = -dy; 

    // --- DEBUG: Seri Porta Yaz (Loglar için) ---
    // printk("Mouse Raw: dx=%d, dy=%d, btn=%d\n", dx, dy, b & 0x07);

    q_push(dx, dy, b & 0x07);
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

// Bunu time.c veya başka bir yerden silip buraya taşıyoruz
void mouse_handler(void) {
    uint8_t status = inb(0x64);

    // 1. Kural: Veri var mı? (Bit 0)
    // 2. Kural: Veri fareye (AUX) mi ait? (Bit 5)
    while (status & 0x01) {
        if (status & 0x20) {
            // Bu gerçek bir fare verisidir
            uint8_t data = inb(0x60);
            ps2_mouse_handle_byte(data);
        } else {
            // Bu klavye verisidir! 
            // BURASI ÇOK KRİTİK: Eğer burada okuma yaparsak klavye çalışır ama fare bozulmaz.
            // Ancak en doğrusu klavye sürücüsünün bunu okumasıdır.
            // Şimdilik sadece fareyi korumak için:
            break; 
        }
        status = inb(0x64);
    }

    outb(0xA0, 0x20);
    outb(0x20, 0x20);
}