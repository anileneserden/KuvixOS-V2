#include "main.designer.h"

// main.c'de tanımlanacak handler
void hello_button_onclick(void* user);

void ui_build(const kvx_api_t* api, void* user_state) {
    if (!api) return;

    // UI elemanlarını kur
    if (api->create_label)
        api->create_label(10, 10, "Hello (designer)");

    if (api->create_button)
        api->create_button(10, 40, 140, 32, "Click me", hello_button_onclick, user_state);
}