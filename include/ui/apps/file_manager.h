#pragma once
#include <app/app.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char cwd[256];

    // items
    struct {
        char     name[64];
        uint32_t size;
        bool     is_dir;
    } items[96];

    int count;
    int selected;

    uint32_t last_click_ms;
    int last_click_index;

    int sidebar_sel;
} file_mgr_t;

extern const app_vtbl_t file_manager_vtbl;

#ifdef __cplusplus
}
#endif