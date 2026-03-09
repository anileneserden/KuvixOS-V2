#pragma once
#include <stdint.h>
#include <stdbool.h>

// 16.16 fixed
typedef int32_t fx;
#define FX_SHIFT 16
#define FX_ONE   (1 << FX_SHIFT)
static inline fx fx_from_int(int v){ return (fx)(v << FX_SHIFT); }
static inline int fx_to_int(fx v){ return (int)(v >> FX_SHIFT); }

struct game_object;

typedef void (*go_update_fn)(struct game_object* self, int dt_ms, void* user);
typedef void (*go_draw_fn)(struct game_object* self, void* user);

typedef struct game_object {
    int id;
    const char* name;

    // transform
    fx x, y;
    fx vx, vy;

    int w, h;
    uint32_t color;

    // callbacks
    go_update_fn on_update;
    go_draw_fn   on_draw;

    // flags
    bool active;
} game_object_t;

// world container (çok basit)
typedef struct {
    game_object_t* objs;
    int cap;
    int count;
    int next_id;
} game_world_t;

void game_world_init(game_world_t* w, game_object_t* storage, int cap);
game_object_t* game_spawn(game_world_t* w, const char* name);
game_object_t* game_find(game_world_t* w, const char* name);

void game_update_all(game_world_t* w, int dt_ms, void* user);
void game_draw_all(game_world_t* w, void* user);