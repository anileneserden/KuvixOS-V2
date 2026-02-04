#include <lib/game_engine.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/wm.h>
#include <kernel/memory/kmalloc.h>
#include <app/app_manager.h>
#include <kernel/time.h>

#define MAX_OBJECTS 64
static game_object_t* object_pool[MAX_OBJECTS];

int engine_win_id = -1;
static app_t* current_app = 0;

// --- TIME SİSTEMİ ---
time_struct_t Time = {0, 0, 0};
static uint32_t _last_frame_tick = 0;

// --- INPUT SİSTEMİ ---
static uint8_t key_map[256];

static int _get_key_impl(keycode_t key) {
    if (key < 256) return key_map[key];
    return 0;
}

const input_t Input = { .GetKey = _get_key_impl };

// --- KEYCODE NAMESPACE ---
const keycode_namespace_t KeyCode = {
    .W = 0x11, .A = 0x1E, .S = 0x1F, .D = 0x20,
    .Space = 0x39, .Escape = 0x01, .LeftShift = 0x2A, .Return = 0x1C,
    .UpArrow = 0x48, .DownArrow = 0x50, .LeftArrow = 0x4B, .RightArrow = 0x4D
};

void engine_internal_set_key(uint16_t key, uint8_t state) {
    uint8_t index = (uint8_t)(key & 0x7F);
    if (index < 256) {
        key_map[index] = state;
    }
}

void engine_update_time() {
    uint32_t current_tick = g_ticks_ms;
    if (_last_frame_tick == 0) _last_frame_tick = current_tick;

    Time.deltaTime = (int)(current_tick - _last_frame_tick);
    Time.time = current_tick;
    Time.frameCount++;
    _last_frame_tick = current_tick;

    if (Time.deltaTime > 100) Time.deltaTime = 100;
}

// --- MOTOR KONTROL ---
void engine_setup(int win_id) {
    engine_win_id = win_id;
    for(int i=0; i<256; i++) key_map[i] = 0;
    for(int i=0; i<MAX_OBJECTS; i++) object_pool[i] = 0;
    
    current_app = appmgr_get_app_by_window_id(win_id);
    kuvix_start();
}

game_object_t* engine_create_object(int x, int y, int w, int h, uint32_t color) {
    for(int i = 0; i < MAX_OBJECTS; i++) {
        if(!object_pool[i]) {
            game_object_t* obj = (game_object_t*)kmalloc(sizeof(game_object_t));
            if(!obj) return 0;
            obj->id = i;
            obj->x = x; obj->y = y;
            obj->w = w; obj->h = h;
            obj->color = color;
            obj->active = 1;
            obj->velocity_y = 0;
            obj->is_grounded = 0;
            object_pool[i] = obj;
            return obj;
        }
    }
    return 0;
}

void engine_loop() {
    if (engine_win_id == -1) return;
    ui_rect_t r = wm_get_client_rect(engine_win_id);
    
    for(int i=0; i<MAX_OBJECTS; i++) {
        if(object_pool[i] && object_pool[i]->active) {
            gfx_fill_rect(r.x + object_pool[i]->x, 
                          r.y + object_pool[i]->y, 
                          object_pool[i]->w, object_pool[i]->h, 
                          object_pool[i]->color);
        }
    }
}