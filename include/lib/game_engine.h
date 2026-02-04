#ifndef GAME_ENGINE_H
#define GAME_ENGINE_H

#include <stdint.h>
#include <app/app.h>

// 1. TEMEL TİPLER VE KEYCODE YAPISI
typedef uint8_t keycode_t;

// Unity'deki KeyCode sınıfını simüle eden yapı
typedef struct {
    keycode_t W, A, S, D;
    keycode_t Space, Escape, LeftShift, Return;
    keycode_t UpArrow, DownArrow, LeftArrow, RightArrow;
} keycode_namespace_t;

extern const keycode_namespace_t KeyCode; // game_engine.c'de tanımlanacak

// 2. TIME YAPISI
typedef struct {
    int deltaTime;      // Milisaniye bazlı (Örn: 16ms)
    uint32_t time;      // Toplam geçen ms
    int frameCount;
} time_struct_t;

extern time_struct_t Time;

// 3. GAME OBJECT YAPISI
typedef struct game_object {
    int id;
    int x, y;
    int w, h;
    uint32_t color;
    int active;
    int velocity_y;
    int is_grounded;
} game_object_t;

// 4. INPUT YAPISI
typedef struct {
    int (*GetKey)(keycode_t key); // Parametreyi keycode_t yaptık
} input_t;

extern const input_t Input;

// 5. MOTOR FONKSİYONLARI
extern int engine_win_id;
void engine_setup(int win_id);
void engine_loop();
void engine_update_time(); // Zamanı her karede güncellemek için şart!
void engine_internal_set_key(uint16_t key, uint8_t state);
game_object_t* engine_create_object(int x, int y, int w, int h, uint32_t color);

// 6. KULLANICI SCRIPT KANCALARI
extern void kuvix_start();
extern void kuvix_update(app_t* app);

#endif