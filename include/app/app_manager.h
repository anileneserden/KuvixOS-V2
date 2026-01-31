// src/lib/app/app_manager.h
#pragma once
#include "app.h"

void appmgr_init(void);

// built-in app başlat
app_t* appmgr_start_demo(void);

app_t* appmgr_start_terminal(void);

// WM event dispatch için:
app_t* appmgr_get_app_by_window_id(int win_id);
