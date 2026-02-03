#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h>
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

/* Basit bir event kuyruğu */
static uint16_t kbd_buffer[256];
static uint8_t head = 0;
static uint8_t tail = 0;

// Shift ve Control gibi niteleyici tuşların durum takibi
static int shift_pressed = 0;

// Harici layout fonksiyonunu prototip olarak ekleyelim
extern const kbd_layout_t* kbd_get_current_layout(void);

void kbd_push_scan_code(uint8_t scancode) {
    uint8_t next = (head + 1) % 256;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

void kbd_init(void) {
    uint8_t status;
    // Açılışta klavye kontrolcüsünü temizle
    while ((status = inb(KBD_STATUS_PORT)) & 0x01) {
        inb(KBD_DATA_PORT);
    }
    
    outb(KBD_STATUS_PORT, 0xAE); // Klavyeyi aktif et
    shift_pressed = 0;
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0;
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 256;
    return code;
}

char kbd_get_char(void) {
    uint16_t scancode = kbd_pop_event();
    if (scancode == 0) return 0;

    // --- SHIFT TAKİBİ ---
    // 0x2A: Sol Shift Basıldı, 0x36: Sağ Shift Basıldı
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return 0;
    }
    // 0xAA: Sol Shift Bırakıldı, 0xB6: Sağ Shift Bırakıldı (Basılan + 0x80)
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return 0;
    }

    // Tuş bırakma (Release) kontrolü: 0x80 biti set edilmişse karakter döndürme
    if (scancode & 0x80) return 0;

    // --- LAYOUT BAĞLANTISI ---
    // layout.c içindeki merkezi fonksiyondan o anki haritayı alıyoruz
    const kbd_layout_t* layout = kbd_get_current_layout();
    
    if (!layout) return 0;

    // Scancode'u temizle (0x7F maskesi ile) ve ilgili tabloya bak
    if (shift_pressed) {
        return (char)layout->shift[scancode & 0x7F];
    } else {
        return (char)layout->normal[scancode & 0x7F];
    }
}

int kbd_has_character(void) {
    return (head != tail);
}

void kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    // Veri var mı (bit 0) VE fare verisi değil mi (bit 5 değil) kontrolü
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(KBD_DATA_PORT);
        kbd_push_scan_code(sc);
    }
}

void kbd_handler(void) {
    kbd_poll();
    // EOI (End of Interrupt) sinyalini Master PIC'e gönder
    outb(0x20, 0x20);
}

/* Linker'ın aradığı kbd_get_key fonksiyonu */
int kbd_get_key(void) {
    // Buffer boşalana kadar karakter ara
    while (kbd_has_character()) {
        char c = kbd_get_char();
        if (c != 0) return (int)c; 
        // c == 0 ise bu bir shift tuşudur, bir sonrakine bak
    }
    return 0;
}