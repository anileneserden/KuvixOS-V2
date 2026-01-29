#include <kernel/kbd.h>
#include <arch/x86/io.h>

#define KBD_Q_SIZE 128
static uint16_t q[KBD_Q_SIZE];
static int q_r = 0, q_w = 0;

static uint8_t shift_active = 0;
static uint8_t caps_lock    = 0;
static uint8_t e0_prefix    = 0;

// --- YENİ EKLENEN FONKSİYON ---
// Shell'in karakter gelip gelmediğini kontrol etmesini sağlar
int kbd_has_character(void) {
    // 1. Donanım seviyesinde yeni bir tuş basılmış mı bak (Polling)
    // Eğer port 0x64'ün 0. biti 1 ise yeni veri var demektir.
    if (inb(0x64) & 0x01) {
        kbd_poll(); // Veriyi oku ve kbd_handle_byte üzerinden kuyruğa (q) at
    }

    // 2. Kuyrukta (buffer) işlenmemiş karakter var mı bak
    return (q_r != q_w);
}

void kbd_init(void) {
    shift_active = 0;
    caps_lock = 0;
    e0_prefix = 0;
    q_r = 0;
    q_w = 0;
}

void kbd_poll(void) {
    // Port 0x64 durumunu kontrol et
    uint8_t status = inb(0x64);
    if (status & 0x01) { // Veri varsa oku
        uint8_t data = inb(0x60);
        if (!(status & 0x20)) { // Mouse verisi değilse işle
            kbd_handle_byte(data);
        }
    }
}

void kbd_handle_byte(uint8_t sc) {
    if (sc == 0xE0) {
        e0_prefix = 1;
        return;
    }

    if (sc & 0x80) {
        uint8_t make = sc & 0x7F;
        if (!e0_prefix && (make == 0x2A || make == 0x36)) {
            shift_active = 0;
        }
        e0_prefix = 0;
        return;
    }

    if (!e0_prefix && (sc == 0x2A || sc == 0x36)) { 
        shift_active = 1; 
        return; 
    }
    if (!e0_prefix && sc == 0x3A) { 
        caps_lock ^= 1; 
        return; 
    }

    if (e0_prefix) {
        e0_prefix = 0;
        return;
    }

    const kbd_layout_t* layout = kbd_get_current_layout();
    if (!layout) return;

    uint8_t c = shift_active ? layout->shift[sc] : layout->normal[sc];
    if (!c) return;

    if (caps_lock && (c >= 'a' && c <= 'z')) {
        c -= 32;
    } else if (caps_lock && (c >= 'A' && c <= 'Z')) {
        c += 32;
    }

    int n = (q_w + 1) % KBD_Q_SIZE;
    if (n != q_r) {
        q[q_w] = (uint16_t)c;
        q_w = n;
    }
}

char kbd_get_char(void) {
    if (q_r == q_w) return 0;
    
    char c = (char)(q[q_r] & 0xFF);
    q_r = (q_r + 1) % KBD_Q_SIZE;
    return c;
}