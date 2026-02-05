#ifndef DESKTOP_ICONS_H
#define DESKTOP_ICONS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int x, y;
    char label[32];
    char vfs_name[64];
    bool is_selected;
    bool is_dir;
    int app_id;
    bool dragging;
    
    // --- YENİ ALANLAR ---
    bool is_editing;     // Şu an adı mı değiştiriliyor?
    char edit_buffer[32]; // Yazılan yeni isim
} desktop_icon_t;

// Temel Yönetim
void desktop_icons_init(void);
void desktop_icons_draw_all(void);
int  desktop_icons_get_hit(int mx, int my);
void desktop_icons_process_click(int index);
void desktop_icons_snap_all(void);
void desktop_icons_set_snap(bool enable);
int desktop_icons_get_count(void);
const char* desktop_icons_get_name(int index);

// Sürükleme Mantığı
void desktop_icons_set_dragging(int index, bool state);
void desktop_icons_move_dragging(int mx, int my);
void desktop_icons_stop_dragging_all(void);

// Seçim (Selection) Mantığı
void desktop_icons_deselect_all(void);
void desktop_icons_select(int index);
void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2);
void desktop_icons_update_selection(int x1, int y1, int x2, int y2);

void desktop_icons_delete_selected(void);

// Diğer fonksiyonların yanına ekle
desktop_icon_t* desktop_icons_get_at(int index);

#endif