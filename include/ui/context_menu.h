#ifndef CONTEXT_MENU_H
#define CONTEXT_MENU_H

#include <stdbool.h>
#include <stdint.h>

// Leaf item
void context_menu_reset(void);
void context_menu_add_item(const char* text, void (*callback)(void));

// ✅ Submenu item: child menu handle döndürür; sonra child'a item ekleyebilirsin
// Örnek:
//   context_menu_t* view = context_menu_add_submenu("Gorunum");
//   context_menu_add_item_to(view, "Dosya uzantilarini goster", cb);
typedef struct context_menu context_menu_t;

context_menu_t* context_menu_add_submenu(const char* text);

// ✅ İstersen child handle'a doğrudan ekleme
void context_menu_add_item_to(context_menu_t* menu, const char* text, void (*callback)(void));
context_menu_t* context_menu_add_submenu_to(context_menu_t* menu, const char* text);

// Show/Hide/Draw/Input
void context_menu_show(int x, int y);
void context_menu_hide(void);
bool context_menu_is_visible(void);
void context_menu_draw(void);
void context_menu_handle_mouse(int mx, int my, bool clicked);

// ✅ (opsiyonel) mouse move hover için (WM sende varsa)
// clicked=false ile handle_mouse zaten hover da günceller ama WM eventin yoksa gerekmez.

#endif