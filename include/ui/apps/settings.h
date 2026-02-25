// include/ui/apps/settings.h
#pragma once

#include <app/app.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SETTINGS_PAGE_GENERAL = 0,
    SETTINGS_PAGE_APPEARANCE,
    SETTINGS_PAGE_STORAGE,
    SETTINGS_PAGE_ABOUT,
    SETTINGS_PAGE_COUNT
} settings_page_t;

typedef struct {
    int page;        // selected page (settings_page_t)
    int scroll;      // future use
} settings_t;

extern const app_vtbl_t settings_vtbl;

#ifdef __cplusplus
}
#endif