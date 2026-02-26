#include <stdint.h>

typedef struct kvx_api {
    void (*log)(const char* s);
} kvx_api_t;

static int kvx_main(const kvx_api_t* api) {
    if (api && api->log) {
        api->log("[HELLO.KEF] Merhaba! KEF calisti.\n");
        api->log("[HELLO.KEF] ikinci satir.\n");
    }
    return 0;
}

// ✅ _start kesin öne gelsin diye özel section
__attribute__((used, section(".text._start")))
int _start(const kvx_api_t* api) {
    return kvx_main(api);
}