#include <ui/debug_screen.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/time.h>

extern volatile uint32_t g_ticks_ms;
extern int kbd_has_character(void);
extern uint16_t kbd_pop_event(void);

// --- RIGIDBODY BİLEŞENLERİ ---
static int posX = 100, posY = 100;
static int velX = 0,   velY = 0;
static int gravity = 2;       // Unity'deki Physics.gravity.y
static int jumpForce = -25;   // Zıplama kuvveti
static int friction = 1;      // Sürtünme
static int groundY = 500;     // Yer (Collision çizgisi)

static int key_states[128] = {0};

void debug_screen_init(void) {
    uint32_t last_frame_ms = g_ticks_ms;

    while (1) {
        // 1. DELTA TIME (Milisaniye bazlı)
        uint32_t current_ms = g_ticks_ms;
        int dt = (int)(current_ms - last_frame_ms);
        last_frame_ms = current_ms;
        if (dt <= 0) dt = 1;

        // 2. INPUT POLLING
        while (kbd_has_character()) {
            uint16_t sc = kbd_pop_event();
            key_states[sc & 0x7F] = (sc & 0x80) ? 0 : 1;
        }

        // 3. PHYSICS UPDATE (Rigidbody Mantığı)
        // Yatay Hareket (A-D)
        if (key_states[0x1E]) velX = -10; // A
        else if (key_states[0x20]) velX = 10; // D
        else {
            // Sürtünme Uygula (Yavaşlayarak durma)
            if (velX > 0) velX -= friction;
            else if (velX < 0) velX += friction;
        }

        // Zıplama (W veya Space)
        if (key_states[0x11] && posY >= groundY - 40) { // Sadece yerdeyken zıpla
            velY = jumpForce;
        }

        // Yerçekimi Uygula (Force)
        velY += gravity;

        // Hızı Pozisyona Ekle (Time.deltaTime ile çarpılmış gibi düşün)
        posX += velX;
        posY += velY;

        // 4. COLLISION DETECTION (Yerle Çarpışma)
        if (posY >= groundY - 40) {
            posY = groundY - 40; // Yerin altına girme
            velY = 0;            // Dikey hızı sıfırla (Grounded)
        }

        // Ekran Kenar Çarpışmaları
        if (posX < 0) posX = 0;
        if (posX > 750) posX = 750;

        // 5. RENDER
        fb_clear(0x050505);

        // "Yer" çizgisini çiz
        gfx_fill_rect(0, groundY, 800, 5, 0xFF0000); 

        // Oyuncu (Rigidbody Kare)
        gfx_fill_rect(posX, posY, 40, 40, 0x00FF00);

        gfx_draw_text(10, 10, 0xFFFFFF, "PHYSICS LAB: WASD to Move & Jump");
        
        fb_present();
        asm volatile("pause");
    }
}