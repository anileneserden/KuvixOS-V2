#include <kernel/exec/kef_json.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

static char* skip_ws(char* p) {
    while (p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

static uint32_t parse_color_hex(const char* s) {
    if (!s || s[0] != '#') return 0xFFFFFF;

    uint32_t v = 0;
    for (int i = 1; i <= 6; i++) {
        char c = s[i];
        v <<= 4;

        if (c >= '0' && c <= '9') v |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (uint32_t)(c - 'A' + 10);
        else return 0xFFFFFF;
    }

    return v;
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

static int widget_type_from_string(const char* s) {
    if (!s) return 0;
    if (strcmp(s, "label") == 0) return KEF_WIDGET_LABEL;
    if (strcmp(s, "button") == 0) return KEF_WIDGET_BUTTON;
    return 0;
}

int kef_json_load_file(const char* path, kef_minimal_state_t* out) {
    if (!path || !out) return 0;

    memset(out, 0, sizeof(*out));
    strcpy(out->title, "KEF JSON");
    out->width = 420;
    out->height = 240;
    out->bg_color = 0xFFFFFF;

        uint8_t buf[1024];
    uint32_t sz = 0;

    if (!vfs_read_all(path, buf, sizeof(buf) - 1, &sz)) {
        printk("[KEFJSON] read failed: %s\n", path);
        return 0;
    }

    buf[sz] = 0;
    char* json = (char*)buf;
    char* p = 0;

    p = strstr(json, "\"title\"");
    if (p) json_copy_string_value(p, out->title, sizeof(out->title));

    p = strstr(json, "\"width\"");
    if (p) json_parse_int_value(p, &out->width);

    p = strstr(json, "\"height\"");
    if (p) json_parse_int_value(p, &out->height);

    p = strstr(json, "\"background-color\"");
    if (p) {
        char color_buf[16];
        memset(color_buf, 0, sizeof(color_buf));

        if (json_copy_string_value(p, color_buf, sizeof(color_buf))) {
            out->bg_color = parse_color_hex(color_buf);
        }
    }

    char* widgets = strstr(json, "\"widgets\"");
    if (widgets) {
        char* cur = widgets;

        while (out->widget_count < KEF_MAX_WIDGETS) {
            char* obj = strstr(cur, "{");
            if (!obj) break;

            char* type_key = strstr(obj, "\"type\"");
            if (!type_key) break;

            kef_widget_t* w = &out->widgets[out->widget_count];
            memset(w, 0, sizeof(*w));
            w->text_color = 0x000000;

            char type_buf[32];
            memset(type_buf, 0, sizeof(type_buf));

            if (!json_copy_string_value(type_key, type_buf, sizeof(type_buf))) {
                break;
            }

            w->type = widget_type_from_string(type_buf);
            if (!w->type) {
                cur = type_key + 1;
                continue;
            }

            char* id_key   = strstr(obj, "\"id\"");
            char* text_key = strstr(obj, "\"text\"");
            char* x_key    = strstr(obj, "\"x\"");
            char* y_key    = strstr(obj, "\"y\"");
            char* w_key    = strstr(obj, "\"w\"");
            char* h_key    = strstr(obj, "\"h\"");
            char* color_key = strstr(obj, "\"color\"");

            if (id_key)   json_copy_string_value(id_key, w->id, sizeof(w->id));
            if (text_key) json_copy_string_value(text_key, w->text, sizeof(w->text));
            if (x_key)    json_parse_int_value(x_key, &w->x);
            if (y_key)    json_parse_int_value(y_key, &w->y);
            if (w_key)    json_parse_int_value(w_key, &w->w);
            if (h_key)    json_parse_int_value(h_key, &w->h);
            
            if (color_key) {
                char color_buf[16];
                memset(color_buf, 0, sizeof(color_buf));

                if (json_copy_string_value(color_key, color_buf, sizeof(color_buf))) {
                    w->text_color = parse_color_hex(color_buf);
                }
            }

            if (w->type == KEF_WIDGET_BUTTON) {
                if (w->w <= 0) w->w = 90;
                if (w->h <= 0) w->h = 28;
            }

            w->owner = out;

            out->widget_count++;
            cur = obj + 1;
        }
    }

    printk("[KEFJSON] loaded title='%s' w=%d h=%d widgets=%d\n",
           out->title, out->width, out->height, out->widget_count);

    return 1;
}