#include <ui/screen.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* -------------------------------------------------- */
/* helpers                                            */
/* -------------------------------------------------- */

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* find_json_string_value(const char* json, const char* key) {
    const char* p = strstr(json, key);
    if (!p) return 0;

    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    p = skip_ws(p);

    if (*p != '"') return 0;
    p++;

    return p;
}

static int find_json_int_value(const char* json, const char* key, int fallback) {
    const char* p = strstr(json, key);
    int sign = 1;
    int v = 0;
    int found = 0;

    if (!p) return fallback;

    p = strchr(p, ':');
    if (!p) return fallback;
    p++;
    p = skip_ws(p);

    if (*p == '-') {
        sign = -1;
        p++;
    }

    while (*p >= '0' && *p <= '9') {
        v = (v * 10) + (*p - '0');
        p++;
        found = 1;
    }

    if (!found) return fallback;
    return v * sign;
}

static int copy_json_string(const char* src, char* out, int out_sz) {
    int i = 0;
    if (!src || !out || out_sz <= 0) return 0;

    while (src[i] && src[i] != '"' && i < out_sz - 1) {
        out[i] = src[i];
        i++;
    }
    out[i] = '\0';
    return i;
}

/* -------------------------------------------------- */
/* color                                              */
/* -------------------------------------------------- */

uint32_t ui_parse_color(const char* s) {
    if (!s || s[0] != '#') return 0x000000;
    if ((int)strlen(s) != 7) return 0x000000; /* #RRGGBB */

    int r1 = hex_val(s[1]), r2 = hex_val(s[2]);
    int g1 = hex_val(s[3]), g2 = hex_val(s[4]);
    int b1 = hex_val(s[5]), b2 = hex_val(s[6]);

    if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
        return 0x000000;

    uint8_t r = (uint8_t)((r1 << 4) | r2);
    uint8_t g = (uint8_t)((g1 << 4) | g2);
    uint8_t b = (uint8_t)((b1 << 4) | b2);

    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* -------------------------------------------------- */
/* panel parse                                        */
/* -------------------------------------------------- */

static int ui_parse_first_panel(const char* json, ui_panel_t* out) {
    const char* children;
    const char* panel_type;
    const char* idv;
    const char* bgv;
    char color_buf[8];

    if (!json || !out) return 0;

    memset(out, 0, sizeof(*out));
    out->visible = 1;
    out->background_color = 0x202020;

    children = strstr(json, "\"children\"");
    if (!children) return 0;

    panel_type = strstr(children, "\"type\": \"Panel\"");
    if (!panel_type) return 0;

    idv = find_json_string_value(panel_type, "\"id\"");
    if (idv) copy_json_string(idv, out->id, sizeof(out->id));

    out->x = find_json_int_value(panel_type, "\"x\"", 0);
    out->y = find_json_int_value(panel_type, "\"y\"", 0);
    out->width = find_json_int_value(panel_type, "\"width\"", 0);
    out->height = find_json_int_value(panel_type, "\"height\"", 0);

    bgv = find_json_string_value(panel_type, "\"backgroundColor\"");
    if (bgv) {
        copy_json_string(bgv, color_buf, sizeof(color_buf));
        out->background_color = ui_parse_color(color_buf);
    }

    out->used = 1;
    return 1;
}

/* -------------------------------------------------- */
/* label parse                                        */
/* -------------------------------------------------- */

static int ui_parse_first_label(const char* json, ui_label_t* out) {
    const char* children;
    const char* label_type;
    const char* idv;
    const char* textv;
    const char* bindv;
    const char* formatv;
    const char* colorv;
    char color_buf[8];

    if (!json || !out) return 0;

    memset(out, 0, sizeof(*out));
    out->visible = 1;
    out->color = 0xFFFFFF;

    children = strstr(json, "\"children\"");
    if (!children) return 0;

    label_type = strstr(children, "\"type\": \"Label\"");
    if (!label_type) return 0;

    idv = find_json_string_value(label_type, "\"id\"");
    if (idv) copy_json_string(idv, out->id, sizeof(out->id));

    out->x = find_json_int_value(label_type, "\"x\"", 0);
    out->y = find_json_int_value(label_type, "\"y\"", 0);

    textv = find_json_string_value(label_type, "\"text\"");
    if (textv) copy_json_string(textv, out->text, sizeof(out->text));

    bindv = find_json_string_value(label_type, "\"bind\"");
    if (bindv) copy_json_string(bindv, out->bind, sizeof(out->bind));

    formatv = find_json_string_value(label_type, "\"format\"");
    if (formatv) copy_json_string(formatv, out->format, sizeof(out->format));

    colorv = find_json_string_value(label_type, "\"color\"");
    if (colorv) {
        copy_json_string(colorv, color_buf, sizeof(color_buf));
        out->color = ui_parse_color(color_buf);
    }

    out->used = 1;
    return 1;
}

/* -------------------------------------------------- */
/* main loader                                        */
/* -------------------------------------------------- */

int ui_screen_load(const char* path, ui_screen_t* out) {
    char buf[4096];
    uint32_t out_sz = 0;
    int ok;
    const char* bgv;
    char color_buf[8];

    if (!path || !out) return 0;

    memset(out, 0, sizeof(*out));
    out->background_color = 0x000000;
    out->loaded = 0;

    ok = vfs_read_all(path, (uint8_t*)buf, sizeof(buf) - 1, &out_sz);
    if (!ok) {
        printk("[ui] read failed: %s\n", path);
        return 0;
    }

    buf[out_sz] = '\0';

    bgv = find_json_string_value(buf, "\"backgroundColor\"");
    if (bgv) {
        copy_json_string(bgv, color_buf, sizeof(color_buf));
        out->background_color = ui_parse_color(color_buf);
    }

    ui_parse_first_panel(buf, &out->panel);
    ui_parse_first_label(buf, &out->label);

    out->loaded = 1;

    printk("[ui] loaded screen: %s\n", path);
    printk("[ui] bg=0x%x panel=%d label=%d\n",
           (unsigned)out->background_color,
           out->panel.used,
           out->label.used);

    return 1;
}