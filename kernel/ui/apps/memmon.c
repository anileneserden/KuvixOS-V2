// kernel/ui/apps/memmon.c
#include <ui/apps/memmon.h>

#include <kernel/memory/kmalloc.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>

// senin projede var:
extern void gfx_draw_text_utf8(int x, int y, uint32_t color, const char* s);

// ------------------------------------------------------------
// state
// ------------------------------------------------------------
static bool g_memmon_visible = false;

void memmon_toggle(void) {
    g_memmon_visible = !g_memmon_visible;
}

bool memmon_is_visible(void) {
    return g_memmon_visible;
}

// mini u32 -> decimal
static void u32_to_str(uint32_t v, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;

    char tmp[16];
    int p = 0;

    if (v == 0) {
        if (cap > 1) { out[0] = '0'; out[1] = 0; }
        return;
    }

    while (v > 0 && p < (int)sizeof(tmp)) {
        tmp[p++] = (char)('0' + (v % 10));
        v /= 10;
    }

    int n = 0;
    while (p > 0 && n + 1 < cap) {
        out[n++] = tmp[--p];
    }
    out[n] = 0;
}

static void draw_kv(int x, int y, const char* k, uint32_t v) {
    char num[16];
    char line[96];

    u32_to_str(v, num, (int)sizeof(num));

    line[0] = 0;
    strncat(line, k, (int)sizeof(line) - 1 - (int)strlen(line));
    strncat(line, num, (int)sizeof(line) - 1 - (int)strlen(line));

    gfx_draw_text_utf8(x, y, 0xFFFFFFFF, line);
}

static void draw_kv_bytes(int x, int y, const char* k, uint32_t bytes) {
    // basit: KB göster
    uint32_t kb = (bytes + 1023) / 1024;

    char num[16];
    char line[96];

    u32_to_str(kb, num, (int)sizeof(num));

    line[0] = 0;
    strncat(line, k, (int)sizeof(line) - 1 - (int)strlen(line));
    strncat(line, num, (int)sizeof(line) - 1 - (int)strlen(line));
    strncat(line, " KB", (int)sizeof(line) - 1 - (int)strlen(line));

    gfx_draw_text_utf8(x, y, 0xFFFFFFFF, line);
}

void memmon_draw(int client_w, int client_h) {
    if (!g_memmon_visible) return;

    // küçük overlay panel (sağ üst)
    int w = 260;
    int h = 140;
    int x = client_w - w - 10;
    int y = 10;

    if (x < 0) x = 0;
    if (y < 0) y = 0;

    // background + border
    gfx_fill_rect(x, y, w, h, 0xCC000000);
    gfx_draw_rect(x, y, w, h, 0xFFFFFFFF);

    gfx_draw_text_utf8(x + 10, y + 10, 0xFFFFFFFF, "MemMon (F12)");

    kmalloc_stats_t st;
    kmalloc_get_stats(&st);

    int ty = y + 32;

    draw_kv_bytes(x + 10, ty + 0,  "Used:   ", st.used_bytes);
    draw_kv_bytes(x + 10, ty + 18, "Free:   ", st.free_bytes);
    draw_kv_bytes(x + 10, ty + 36, "Largest:", st.largest_free);

    draw_kv(x + 10, ty + 60, "Alloc:  ", st.alloc_count);
    draw_kv(x + 10, ty + 78, "FreeC:  ", st.free_count);

    // ufak uyarı: fragmentation
    if (st.free_bytes > 0 && st.largest_free < (st.free_bytes / 4)) {
        gfx_draw_text_utf8(x + 10, y + h - 22, 0xFFFFCC00, "Warn: fragmented heap");
    }
}