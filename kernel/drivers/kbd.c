#include <kernel/kbd.h>
#include <arch/x86/io.h>

#define KBD_Q_SIZE 128
static uint16_t q[KBD_Q_SIZE];
static int q_r = 0, q_w = 0;

static uint8_t shift_active = 0;
static uint8_t caps_lock    = 0;
static uint8_t e0_prefix    = 0;

void kbd_init(void) {
    shift_active = 0;
    caps_lock = 0;
    e0_prefix = 0;
    q_r = 0;
    q_w = 0;
}

void kbd_poll(void) {
    // Port 0x64 durumunu kontrol et
    for (int i = 0; i < 32; i++) {
        uint8_t status = inb(0x64);
        if (!(status & 0x01)) break; // Veri yoksa çık
        
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

    // Tuş bırakma (Break code) kontrolü
    if (sc & 0x80) {
        uint8_t make = sc & 0x7F;
        if (!e0_prefix && (make == 0x2A || make == 0x36)) {
            shift_active = 0;
        }
        e0_prefix = 0;
        return;
    }

    // Tuş basma (Make code)
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
        return; // Ok tuşları vb. şimdilik pas geçildi
    }

    // Aktif layout pointer'ını alıyoruz (layout.c içindeki motoru tetikler)
    const kbd_layout_t* layout = kbd_get_current_layout();
    
    // Artık sabit diziden değil, o anki layout'un içindeki diziden okuyoruz
    uint8_t c = shift_active ? layout->shift[sc] : layout->normal[sc];
    if (!c) return;

    // Harf ise Caps Lock kontrolü
    if (caps_lock && (c >= 'a' && c <= 'z')) {
        c -= 32;
    } else if (caps_lock && (c >= 'A' && c <= 'Z')) {
        c += 32;
    }

    // Kuyruğa (Buffer) ekle
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