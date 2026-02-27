// kernel/ui/apps/kuvix_browser.h
#pragma once
#include <stdint.h>

#define KBROWSER_URL_MAX     160
#define KBROWSER_MAX_TABS    3
#define KBROWSER_HISTORY_MAX 8

typedef struct {
    // tabs
    int active_tab;

    // url editing
    char url[KBROWSER_URL_MAX];
    int  url_len;
    int  addr_edit_mode;

    // history (demo)
    char history[KBROWSER_HISTORY_MAX][KBROWSER_URL_MAX];
    int  history_count;
    int  history_index;

    // status
    char status[64];

    // basic layout cache (client coords)
    int cx, cy, cw, ch;

} kuvix_browser_t;

// vtbl dışarıya
struct app_vtbl;
extern const struct app_vtbl kuvix_browser_vtbl;