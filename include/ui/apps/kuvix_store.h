// kernel/ui/apps/kuvix_store.h
#ifndef KUVIX_STORE_H
#define KUVIX_STORE_H

#include <app/app.h>
#include <stdbool.h>

#define STORE_MAX_APPS   16
#define STORE_TITLE_MAX  64
#define STORE_PATH_MAX   128

#define STORE_DESC_MAX   128
#define STORE_ICON_MAX   128

typedef struct {
    // ✅ app->user küçük olacak
    int selected;
    int scroll;
    int count; // opsiyonel: UI kolaylığı için
} kuvix_store_t;

// ✅ item listesi app->user içinde değil, kuvix_store.c içinde global olacak
typedef struct {
    char title[STORE_TITLE_MAX];
    char kapp_path[STORE_PATH_MAX];
    int  app_id;
    bool valid;
} store_item_t;

extern const app_vtbl_t kuvix_store_vtbl;

#endif