#include <kuvixos.h>

static const kvx_api_t* g_kvx_api = 0;

extern "C" void kvx_sdk_init(const kvx_api_t* api) {
    g_kvx_api = api;
}

extern "C" void print(const char* text) {
    if (!g_kvx_api || !g_kvx_api->print || !text) return;
    g_kvx_api->print(text);
}

extern "C" int kvx_argc(void) {
    if (!g_kvx_api || !g_kvx_api->arg_count) return 0;
    return g_kvx_api->arg_count();
}

extern "C" const char* kvx_argv(int index) {
    if (!g_kvx_api || !g_kvx_api->arg_at) return "";
    return g_kvx_api->arg_at(index);
}

extern "C" int kvx_file_read_all(const char* path, char* out, uint32_t cap, uint32_t* out_len) {
    if (!g_kvx_api || !g_kvx_api->file_read_all) return 0;
    return g_kvx_api->file_read_all(path, out, cap, out_len);
}

extern "C" int main(void);

extern "C" int kvx_entry(const kvx_api_t* api, kvx_kef_app_t* out_app) {
    kvx_sdk_init(api);

    if (out_app) {
        out_app->kind = KVX_APP_KIND_CONSOLE;
        out_app->on_draw = 0;
        out_app->ui_json = 0;
        out_app->ui_json_size = 0;
    }

    return main();
}