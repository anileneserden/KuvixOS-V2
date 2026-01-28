#include "service.h"
#include <kernel/printk.h>

/* ---------- mini string utils (libc olmadığı için) ---------- */
static int streq(const char* a, const char* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == 0 && *b == 0);
}

/* Dışarıdan (service_registry.c içinden) gelen servis tablosu */
extern kv_service_t g_services[];
extern int g_service_count;

void services_init(void) {
    // İleride buraya başlangıçta çalışması gereken servisler eklenebilir.
}

kv_service_t* service_find(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < g_service_count; i++) {
        if (streq(g_services[i].name, name)) return &g_services[i];
    }
    return 0;
}

static int svc_query_running(kv_service_t* s) {
    if (!s) return 0;
    if (s->status) {
        int r = s->status();
        if (r < 0) return (s->state == SVC_RUNNING);
        return (r ? 1 : 0);
    }
    return (s->state == SVC_RUNNING);
}

int service_is_running(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) return 0;
    return svc_query_running(s);
}

int service_start(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) return -1;
    if (svc_query_running(s)) return 0;
    if (!s->start) return -2;

    int r = s->start();
    if (r == 0) s->state = SVC_RUNNING;
    return r;
}

int service_stop(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) return -1;
    if (!svc_query_running(s)) return 0;
    if (!s->stop) return -2;

    int r = s->stop();
    if (r == 0) s->state = SVC_STOPPED;
    return r;
}

int service_restart(const char* name) {
    int r1 = service_stop(name);
    if (r1 < 0) return r1;
    return service_start(name);
}

int service_enable(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) return -1;
    s->enabled = 1;
    return 0;
}

int service_disable(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) return -1;
    s->enabled = 0;
    return 0;
}

/* -------- Printing (V2 Modeli) -------- */

static void print_status_line(kv_service_t* s) {
    if (!s) return;

    // Tek bir printk çağrısı ile formatlı çıktı alıyoruz
    printk("%s - %s | %s | %s\n", 
           s->name, 
           s->desc ? s->desc : "No description", 
           svc_query_running(s) ? "running" : "stopped",
           s->enabled ? "enabled" : "disabled");
}

void services_list(void) {
    printk("KuvixOS Service List:\n");
    printk("----------------------------------\n");
    for (int i = 0; i < g_service_count; i++) {
        print_status_line(&g_services[i]);
    }
}

void service_print_status(const char* name) {
    kv_service_t* s = service_find(name);
    if (!s) {
        printk("Service not found: %s\n", name);
        return;
    }
    print_status_line(s);
}