// kernel/ui/apps/fileman_v2.h
#pragma once
#include <stdint.h>

#define FM2_PATH_MAX   256
#define FM2_NAME_MAX    64
#define FM2_MAX_ITEMS  256

typedef struct {
    char name[FM2_NAME_MAX];
    char full[FM2_PATH_MAX];
    int  is_dir;
} fm2_item_t;

typedef struct {
    // current folder (committed)
    char path[FM2_PATH_MAX];

    fm2_item_t items[FM2_MAX_ITEMS];
    int item_count;

    // selection
    int selected;

    // scrolling
    int scroll_y;

    // double click detection
    int last_click_index;
    uint32_t last_click_tick_ms;

    // cached client size
    int cw, ch;

    // status text
    char status[64];

    // --- path bar edit mode (draft buffer) ---
    int  path_edit_mode;
    char path_buf[FM2_PATH_MAX];
    int  path_len;
} fileman_v2_t;

// vtbl dışarıya
struct app_vtbl;
extern const struct app_vtbl fileman_v2_vtbl;