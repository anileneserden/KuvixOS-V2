// kernel/ui/apps/kuvix_browser.h
#pragma once
#include <stdint.h>

#define KBROWSER_URL_MAX     160
#define KBROWSER_MAX_TABS    3
#define KBROWSER_HISTORY_MAX 8

typedef struct {
    int active_tab;

    char url[KBROWSER_URL_MAX];
    int  url_len;
    int  addr_edit_mode;

    char history[KBROWSER_HISTORY_MAX][KBROWSER_URL_MAX];
    int  history_count;
    int  history_index;

    char status[64];

    int scroll_y;

    int cx, cy, cw, ch;
} kuvix_browser_t;

// vtbl dışarıya
struct app_vtbl;
extern const struct app_vtbl kuvix_browser_vtbl;

struct app;
void kuvix_browser_open_url(struct app* app, const char* url);