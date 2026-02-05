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

extern kbd_layout_t layout_trq;
extern kbd_layout_t layout_us;
static kbd_layout_t* current_layout = &layout_us;

static int ctrl_pressed = 0; // 0: Basılı değil, 1: Basılı
static int ctrl_state = 0;

// Bu fonksiyonu dosyanın herhangi bir yerine (ama kbd_push_scan_code'un dışına) ekle
int kbd_is_ctrl_pressed(void) {
    return ctrl_state;
}

void kbd_push_scan_code(uint8_t scancode) {
    // CTRL tuşu kontrolü (PS/2 Set 1)
    // 0x1D: Press, 0x9D: Release
    if (scancode == 0x1D) {
        ctrl_state = 1;
    } else if (scancode == 0x9D) {
        ctrl_state = 0;
    }

    uint8_t next = (head + 1) % 256;
    if (next != tail) {
        kbd_buffer[head] = scancode;
        head = next;
    }
}

void kbd_init(void) {
    // TEMİZLİK: Sadece klavye verilerini temizle, fare verilerine dokunma
    uint8_t status;
    while ((status = inb(KBD_STATUS_PORT)) & 0x01) {
        uint8_t data = inb(KBD_DATA_PORT);
        // Eğer bu veri fareye (AUX) aitse (bit 5 set), fare sürücüsü için bırakılabilir 
        // veya ilk açılışta her şey temizlensin diye hepsi okunabilir.
        // Genelde init aşamasında her şeyi yutmak en güvenlisidir.
    }
    
    outb(KBD_STATUS_PORT, 0xAE); // Klavyeyi aktif et
    current_layout = &layout_trq; 
}

uint16_t kbd_pop_event(void) {
    if (head == tail) return 0;
    uint16_t code = kbd_buffer[tail];
    tail = (tail + 1) % 256;
    return code;
}

char kbd_get_char(void) {
    uint16_t scancode = kbd_pop_event();
    if (scancode == 0 || (scancode & 0x80)) return 0; // Release (bırakma) bitini kontrol et
    return current_layout->normal[scancode & 0x7F];
}

int kbd_has_character(void) {
    return (head != tail);
}

/**
 * @brief Donanım portunu kontrol eder. 
 * Çakışmayı önlemek için 5. biti kontrol eder.
 */
void kbd_poll(void) {
    uint8_t status = inb(KBD_STATUS_PORT);

    // KURAL: Veri var mı (bit 0 == 1) VE bu veri fareye mi ait (bit 5 == 1)?
    // Eğer bit 5 set edilmişse, bu veri fare verisidir. Klavyeden okuma yapma!
    if ((status & 0x01) && !(status & 0x20)) {
        uint8_t sc = inb(KBD_DATA_PORT);
        kbd_push_scan_code(sc);
    }
}

// Assembly'deki "call kbd_handler" burayı çalıştıracak
void kbd_handler(void) {
    // Mevcut polling mantığını kesme geldiğinde de kullanalım
    kbd_poll();

    // Kesmenin bittiğini PIC'e bildir (Master PIC için)
    // Eğer bunu yapmazsan ilk tuş basışından sonra klavye kilitlenir
    outb(0x20, 0x20);
}

// notepad.c ve diğer uygulamaların scancode'u ASCII'ye çevirmesi için
char kbd_scancode_to_ascii(uint8_t scancode) {
    if (scancode & 0x80) return 0; // Tuş bırakma kodlarını yoksay
    if (!current_layout) return 0;
    
    return current_layout->normal[scancode & 0x7F];
}