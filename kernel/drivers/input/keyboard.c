#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h>
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

/* Tampon boyutunu artırdık */
static uint16_t kbd_buffer[512];
static volatile uint16_t head = 0;
static volatile uint16_t tail = 0;

extern kbd_layout_t layout_trq;
extern kbd_layout_t layout_us;
static kbd_layout_t* current_layout = &layout_us;

void kbd_push_scan_code(uint8_t scancode) {
    uint16_t next = (head + 1) % 512;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

void kbd_init(void) {
    // Portu temizle
    while (inb(KBD_STATUS_PORT) & 0x01) {
        inb(KBD_DATA_PORT);
    }
    
    outb(KBD_STATUS_PORT, 0xAE); // Klavyeyi aktif et
    current_layout = &layout_trq; 
}

// kernel/drivers/input/keyboard.c içinde

void kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    // Eğer veri varsa (bit 0) VE bu veri fareye ait değilse (bit 5 değilse)
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(KBD_DATA_PORT);
        kbd_push_scan_code(sc);
    }
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0;
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 512;
    return code;
}

char kbd_get_char(void) {
    uint16_t scancode = kbd_pop_event();
    if (scancode == 0) return 0;
    
    // Release bitini ayıkla (0x80)
    uint8_t key_only = scancode & 0x7F;
    if (scancode & 0x80) return 0; 

    // Layout sınır kontrolü (Kritik: A tuşu 0x1E'dir)
    if (key_only < 128) {
        return current_layout->normal[key_only];
    }
    return 0;
}

int kbd_has_character(void) {
    return (head != tail);
}

// ASIL DEĞİŞİKLİK BURADA: Polling ve Interrupt ayrılmalı
void kbd_handler(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    // Bit 0: Veri var mı?
    // Bit 5: Veri FAREYE mi ait? (1 ise fare, 0 ise klavye)
    if (status & 0x01) {
        uint8_t data = inb(KBD_DATA_PORT);
        
        if (!(status & 0x20)) {
            // Sadece fare verisi değilse kuyruğa ekle
            kbd_push_scan_code(data);
        }
    }

    // PIC'e "işlem tamam" de
    outb(0x20, 0x20);
}

// kbd_poll artık gereksiz, çünkü kbd_handler zaten kesme ile çalışıyor.
// Eğer sistemin polling gerektiriyorsa kbd_handler içindeki mantığı kullanabilirsin.