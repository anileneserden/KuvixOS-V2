#pragma once
#include <stdint.h>

typedef enum {
    SVC_STOPPED = 0,
    SVC_RUNNING = 1,
} svc_state_t;

typedef struct kv_service {
    const char* name;           // "kv_desktop"
    const char* desc;           // açıklama

    int (*start)(void);         // 0 ok, <0 fail
    int (*stop)(void);          // 0 ok, <0 fail

    // opsiyonel: null olabilir
    int (*status)(void);        // 1 running, 0 stopped, <0 unknown

    svc_state_t state;
    uint8_t enabled;            // boot autostart için (şimdilik sadece flag)
} kv_service_t;

/* registry */
void services_init(void);

/* find */
kv_service_t* service_find(const char* name);

/* operations */
int service_start(const char* name);
int service_stop(const char* name);
int service_restart(const char* name);
int service_is_running(const char* name);   // 1/0

/* enable flags */
int service_enable(const char* name);
int service_disable(const char* name);

/* print helpers (print/println kullanır) */
void services_list(void);
void service_print_status(const char* name);
