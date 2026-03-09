#include <game/gameobject.h>
#include <lib/string.h>

void game_world_init(game_world_t* w, game_object_t* storage, int cap) {
    w->objs = storage;
    w->cap = cap;
    w->count = 0;
    w->next_id = 1;
    for (int i = 0; i < cap; i++) {
        w->objs[i].active = false;
    }
}

game_object_t* game_spawn(game_world_t* w, const char* name) {
    if (!w || w->count >= w->cap) return 0;

    // ilk boş slot
    for (int i = 0; i < w->cap; i++) {
        if (!w->objs[i].active) {
            game_object_t* o = &w->objs[i];
            memset(o, 0, sizeof(*o));
            o->active = true;
            o->id = w->next_id++;
            o->name = name;
            w->count++;
            return o;
        }
    }
    return 0;
}

game_object_t* game_find(game_world_t* w, const char* name) {
    if (!w || !name) return 0;
    for (int i = 0; i < w->cap; i++) {
        game_object_t* o = &w->objs[i];
        if (o->active && o->name && strcmp(o->name, name) == 0) return o;
    }
    return 0;
}

void game_update_all(game_world_t* w, int dt_ms, void* user) {
    if (!w) return;
    for (int i = 0; i < w->cap; i++) {
        game_object_t* o = &w->objs[i];
        if (!o->active) continue;
        if (o->on_update) o->on_update(o, dt_ms, user);
    }
}

void game_draw_all(game_world_t* w, void* user) {
    if (!w) return;
    for (int i = 0; i < w->cap; i++) {
        game_object_t* o = &w->objs[i];
        if (!o->active) continue;
        if (o->on_draw) o->on_draw(o, user);
    }
}