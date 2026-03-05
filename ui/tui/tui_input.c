#include <ui/tui/tui_input.h>
#include <ui/tui/tui_action.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/fs/vfs.h>

#include <lib/string.h>
#include <stdint.h>

#define IN_TITLE_MAX   64
#define IN_PATH_MAX    128
#define IN_KEY_MAX     64
#define IN_NEXT_MAX    128
#define IN_BUF_MAX     64

static int  g_active = 0;
static int  g_e0 = 0;

static char g_title[IN_TITLE_MAX];
static char g_path[IN_PATH_MAX];
static char g_key[IN_KEY_MAX];
static char g_next[IN_NEXT_MAX];

static char g_buf[IN_BUF_MAX];
static int  g_len = 0;

static int  g_dirty = 1;

static void strset(char* dst, int cap, const char* s) {
    if (!dst || cap <= 0) return;
    if (!s) s = "";
    strncpy(dst, s, cap - 1);
    dst[cap - 1] = 0;
}

static void draw_center_box(const char* title, const char* line1, const char* line2) {
    gfx_clear(0x00202020);

    int box_w = 420;
    int box_h = 200;
    int x = ((int)fb_get_width()  - box_w) / 2;
    int y = ((int)fb_get_height() - box_h) / 2;

    gfx_fill_rect(x, y, box_w, box_h, 0x00262626);
    gfx_draw_rect(x, y, box_w, box_h, 0x00404040);

    gfx_draw_text_utf8(x + 20, y + 18, 0x00FFFFFF, title ? title : "Input");
    gfx_draw_text_utf8(x + 20, y + 60, 0x00AAAAAA, line1 ? line1 : "");
    gfx_draw_text_utf8(x + 20, y + 92, 0x00FFFFFF, line2 ? line2 : "");

    fb_present();
}

static int is_space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }

static int kv_update_text(const char* in, uint32_t in_sz,
                          const char* key, const char* value,
                          char* out, uint32_t out_cap, uint32_t* out_sz) {
    if (!out || out_cap == 0) return 0;
    if (!key) key = "";
    if (!value) value = "";

    uint32_t o = 0;
    int replaced = 0;

    // satır satır işle
    uint32_t i = 0;
    while (i < in_sz) {
        // line start
        uint32_t ls = i;
        while (i < in_sz && in[i] != '\n') i++;
        uint32_t le = i; // '\n' hariç
        if (i < in_sz && in[i] == '\n') i++; // skip '\n'

        // satırı trimlemeden key= mi kontrol et
        // format: key=...
        // baştaki boşlukları yok sayalım
        uint32_t p = ls;
        while (p < le && is_space(in[p])) p++;

        int match = 0;
        // key compare
        uint32_t k = 0;
        uint32_t pp = p;
        while (pp < le && key[k] && in[pp] == key[k]) { pp++; k++; }
        if (key[k] == 0) {
            // key bitti, şimdi '=' olmalı
            if (pp < le && in[pp] == '=') match = 1;
        }

        if (match && !replaced) {
            // key=value yaz
            char line[256];
            int n = 0;

            // key
            for (int kk = 0; key[kk] && n < (int)sizeof(line)-1; kk++) line[n++] = key[kk];
            if (n < (int)sizeof(line)-1) line[n++] = '=';
            for (int vv = 0; value[vv] && n < (int)sizeof(line)-1; vv++) line[n++] = value[vv];
            if (n < (int)sizeof(line)-1) line[n++] = '\n';
            line[n] = 0;

            if (o + (uint32_t)n >= out_cap) return 0;
            memcpy(out + o, line, (uint32_t)n);
            o += (uint32_t)n;

            replaced = 1;
        } else {
            // satırı olduğu gibi kopyala (newline dahil)
            uint32_t seg = (le - ls);
            // newline
            if (i <= in_sz && (le < i)) seg += 1;

            if (o + seg >= out_cap) return 0;
            memcpy(out + o, in + ls, seg);
            o += seg;
        }
    }

    if (!replaced) {
        // sona ekle
        char line[256];
        int n = 0;
        for (int kk = 0; key[kk] && n < (int)sizeof(line)-1; kk++) line[n++] = key[kk];
        if (n < (int)sizeof(line)-1) line[n++] = '=';
        for (int vv = 0; value[vv] && n < (int)sizeof(line)-1; vv++) line[n++] = value[vv];
        if (n < (int)sizeof(line)-1) line[n++] = '\n';
        line[n] = 0;

        if (o + (uint32_t)n >= out_cap) return 0;
        memcpy(out + o, line, (uint32_t)n);
        o += (uint32_t)n;
    }

    if (out_sz) *out_sz = o;
    return 1;
}

static void save_value_and_exit(void) {
    // read existing
    uint8_t inbuf[1024];
    uint32_t insz = 0;
    int ok = vfs_read_all(g_path, inbuf, (uint32_t)sizeof(inbuf), &insz);
    if (!ok) { insz = 0; }

    char outbuf[1024];
    uint32_t outsz = 0;

    if (!kv_update_text((const char*)inbuf, insz, g_key, g_buf, outbuf, (uint32_t)sizeof(outbuf), &outsz)) {
        // update failed -> yine de direkt overwrite deneyelim (tek satır)
        char line[256];
        int n = 0;
        for (int kk = 0; g_key[kk] && n < (int)sizeof(line)-1; kk++) line[n++] = g_key[kk];
        if (n < (int)sizeof(line)-1) line[n++] = '=';
        for (int vv = 0; g_buf[vv] && n < (int)sizeof(line)-1; vv++) line[n++] = g_buf[vv];
        if (n < (int)sizeof(line)-1) line[n++] = '\n';
        outsz = (uint32_t)n;
        memcpy(outbuf, line, outsz);
    }

    (void)vfs_write_all(g_path, (const uint8_t*)outbuf, outsz);

    // çık
    g_active = 0;
    g_dirty = 1;

    // next action çalıştır
    if (g_next[0]) tui_execute_action(g_next);
}

static void cancel_and_exit(void) {
    g_active = 0;
    g_dirty = 1;
    if (g_next[0]) tui_execute_action(g_next);
}

void tui_input_begin(const char* title,
                     const char* path,
                     const char* key,
                     const char* next_action,
                     const char* initial) {
    strset(g_title, sizeof(g_title), title ? title : "Input");
    strset(g_path,  sizeof(g_path),  path  ? path  : "/system/user.conf");
    strset(g_key,   sizeof(g_key),   key   ? key   : "value");
    strset(g_next,  sizeof(g_next),  next_action ? next_action : "");

    strset(g_buf, sizeof(g_buf), initial ? initial : "");
    g_len = (int)strlen(g_buf);
    if (g_len < 0) g_len = 0;
    if (g_len >= IN_BUF_MAX) g_len = IN_BUF_MAX - 1;

    g_e0 = 0;
    g_active = 1;
    g_dirty = 1;
}

int tui_input_is_active(void) { return g_active; }

void tui_input_draw(void) {
    if (!g_active) return;

    // alt satıra cursor ekle
    char line2[IN_BUF_MAX + 4];
    int p = 0;
    for (int i = 0; g_buf[i] && p < (int)sizeof(line2)-2; i++) line2[p++] = g_buf[i];
    line2[p++] = '_';
    line2[p] = 0;

    draw_center_box(g_title,
                    "Type value and press ENTER (ESC cancels)",
                    line2);

    g_dirty = 0;
}

void tui_input_tick(void) {
    if (!g_active) return;
    if (g_dirty) tui_input_draw();
}

void tui_input_handle_scancode(uint16_t sc) {
    if (!g_active) return;

    uint8_t code = (uint8_t)(sc & 0xFF);

    // E0 prefix
    if (code == 0xE0) { g_e0 = 1; return; }

    // break ignore
    if (code & 0x80) { g_e0 = 0; return; }

    // (şimdilik) E0'lu tuşları yok say
    if (g_e0) { g_e0 = 0; return; }

    // ESC = cancel
    if (code == 0x01) {
        cancel_and_exit();
        return;
    }

    // ENTER = save
    if (code == 0x1C) {
        save_value_and_exit();
        return;
    }

    // BACKSPACE
    // (kbd_scancode_to_ascii bazen '\b' döndürür; bazen 0x0E)
    if (code == 0x0E) {
        if (g_len > 0) {
            g_len--;
            g_buf[g_len] = 0;
            g_dirty = 1;
        }
        return;
    }

    char c = kbd_scancode_to_ascii(code);
    if (!c) return;

    if ((uint8_t)c == 8 || (uint8_t)c == 127 || c == '\b') {
        if (g_len > 0) {
            g_len--;
            g_buf[g_len] = 0;
            g_dirty = 1;
        }
        return;
    }

    // printable
    uint8_t uc = (uint8_t)c;
    if (uc >= 32) {
        if (g_len < IN_BUF_MAX - 1) {
            g_buf[g_len++] = (char)uc;
            g_buf[g_len] = 0;
            g_dirty = 1;
        }
    }
}