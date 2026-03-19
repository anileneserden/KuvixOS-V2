#include <kernel/exec/kef_json.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <stdint.h>

static char* skip_ws(char* p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static int json_copy_string_value(char* from, char* out, int cap) {
    if (!from || !out || cap <= 0) return 0;

    char* colon = strstr(from, ":");
    if (!colon) return 0;

    char* q1 = strstr(colon, "\"");
    if (!q1) return 0;
    q1++;

    char* q2 = strstr(q1, "\"");
    if (!q2) return 0;

    int len = (int)(q2 - q1);
    if (len >= cap) len = cap - 1;

    memcpy(out, q1, (size_t)len);
    out[len] = 0;
    return 1;
}

static int json_parse_int_value(char* from, int* outv) {
    if (!from || !outv) return 0;

    char* colon = strstr(from, ":");
    if (!colon) return 0;

    char* p = skip_ws(colon + 1);
    if (!p) return 0;

    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }

    if (*p < '0' || *p > '9') return 0;

    int v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (*p - '0');
        p++;
    }

    *outv = v * sign;
    return 1;
}

int kef_json_load_file(const char* path, kef_minimal_state_t* out) {
    if (!path || !out) return 0;

    memset(out, 0, sizeof(*out));
    out->width = 420;
    out->height = 240;
    strcpy(out->title, "KEF JSON");

    uint8_t* buf = 0;
    uint32_t sz = 0;

    if (!vfs_read_all_alloc(path, &buf, &sz) || !buf) {
        printk("[KEFJSON] read failed: %s\n", path);
        return 0;
    }

    char* json = (char*)buf;

    char* p = 0;

    p = strstr(json, "\"title\"");
    if (p) json_copy_string_value(p, out->title, sizeof(out->title));

    p = strstr(json, "\"width\"");
    if (p) json_parse_int_value(p, &out->width);

    p = strstr(json, "\"height\"");
    if (p) json_parse_int_value(p, &out->height);

    char* widgets = strstr(json, "\"widgets\"");
    if (widgets) {
        char* w = widgets;

        while (out->widget_count < KEF_MAX_WIDGETS) {
            char* type_key = strstr(w, "\"type\"");
            if (!type_key) break;

            kef_widget_t* kw = &out->widgets[out->widget_count];
            memset(kw, 0, sizeof(*kw));

            if (!json_copy_string_value(type_key, kw->type, sizeof(kw->type))) {
                break;
            }

            char* next_obj = strstr(type_key + 1, "{");
            char* text_key = strstr(type_key, "\"text\"");
            char* x_key    = strstr(type_key, "\"x\"");
            char* y_key    = strstr(type_key, "\"y\"");

            if (text_key) json_copy_string_value(text_key, kw->text, sizeof(kw->text));
            if (x_key)    json_parse_int_value(x_key, &kw->x);
            if (y_key)    json_parse_int_value(y_key, &kw->y);

            out->widget_count++;

            if (!next_obj) break;
            w = type_key + 1;
        }
    }

    vfs_free_alloc(buf);

    printk("[KEFJSON] loaded title='%s' w=%d h=%d widgets=%d\n",
           out->title, out->width, out->height, out->widget_count);

    return 1;
}