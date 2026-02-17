// src/lib/app/app_manager.h
#pragma once
#include "app.h"

// include/app/app_manager.h
void appmgr_init(void);
app_t* appmgr_start_app(int app_id); // ID ile başlatan yeni fonksiyon
app_t* appmgr_start_path(const char* path);
app_t* appmgr_open_path(const char* path);

// WM event dispatch için:
app_t* appmgr_get_app_by_window_id(int win_id);
app_t* appmgr_get_app_by_id(int app_id);