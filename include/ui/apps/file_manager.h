#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <app/app.h>

// File Manager Uygulama Yapısı
void file_manager_init(app_t* app);
void file_manager_draw(app_t* app);
void file_manager_handle_mouse(app_t* app, int mx, int my, uint8_t pressed);

#endif