#include <ui/screen.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* --- helpers --- */

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* --- color parse --- */

uint32_t ui_parse_color(const char* s) {
    if (!s || s[0] != '#') return 0x000000;

    int len = (int)strlen(s);
    if (len != 7) return 0x000000; // #RRGGBB

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

/* --- JSON helper --- */

static const char* find_json_string_value(const char* json, const char* key) {
    const char* p = strstr(json, key);
    if (!p) return 0;

    p = strchr(p, ':');
    if (!p) return 0;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    if (*p != '"') return 0;
    p++;

    return p;
}

/* --- main loader --- */

int ui_screen_load(const char* path, ui_screen_t* out) {
    if (!path || !out) return 0;

    out->background_color = 0x000000;
    out->loaded = 0;

    char buf[1024];
    uint32_t out_sz = 0;

    int ok = vfs_read_all(path, (uint8_t*)buf, sizeof(buf) - 1, &out_sz);
    if (!ok) {
        printk("[ui] read failed: %s\n", path);
        return 0;
    }

    buf[out_sz] = '\0';

    const char* val = find_json_string_value(buf, "\"backgroundColor\"");
    if (!val) {
        printk("[ui] backgroundColor not found\n");
        out->loaded = 1;
        return 1;
    }

    char color[8];
    int i = 0;

    while (val[i] && val[i] != '"' && i < 7) {
        color[i] = val[i];
        i++;
    }
    color[i] = '\0';

    out->background_color = ui_parse_color(color);
    out->loaded = 1;

    printk("[ui] loaded screen: %s (%s)\n", path, color);
    return 1;
}