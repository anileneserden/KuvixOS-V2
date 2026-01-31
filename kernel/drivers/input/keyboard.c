#include <kernel/drivers/input/keyboard.h>
#include <kernel/kbd.h>
#include <arch/x86/io.h> // outb/inb için
#include <stdint.h>

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

/* Basit bir event kuyruğu (Ring Buffer) */
static uint16_t kbd_buffer[256];
static uint8_t head = 0;
static uint8_t tail = 0;

extern kbd_layout_t layout_trq;
extern kbd_layout_t layout_us;
static kbd_layout_t* current_layout = &layout_us;

/* Kuyruğa veri ekle (Interrupt Handler tarafından çağrılır) */
void kbd_push_scan_code(uint8_t scancode) {
    uint8_t next = (head + 1) % 256;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

void kbd_init(void) {
    // PS/2 Controller'ı temizle ve klavyeyi aktif et
    while (inb(KBD_STATUS_PORT) & 0x01) inb(KBD_DATA_PORT); // Buffer temizliği
    outb(KBD_STATUS_PORT, 0xAE); // Klavyeyi aktif et (Enable KBD)
    
    current_layout = &layout_trq; // Varsayılan TR-Q
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0; // Kuyruk boş
    
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 256;
    return code;
}

/* Shell için karakter dönüşümü */
char kbd_get_char(void) {
    uint16_t scancode = kbd_pop_event();
    if (scancode == 0 || scancode > 128) return 0; // Şimdilik sadece basılma (press)
    
    // Mevcut layout üzerinden karakteri bul
    return current_layout->normal[scancode];
}

int kbd_has_character(void) {
    return (head != tail);
}

void kbd_poll(void) {
    // Eğer IRQ (kesme) ayarlı değilse, portu manuel kontrol et
    if (inb(KBD_STATUS_PORT) & 0x01) {
        uint8_t sc = inb(KBD_DATA_PORT);
        kbd_push_scan_code(sc);
    }
}