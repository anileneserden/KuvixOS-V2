#include <lib/tty_progress.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/fb_console.h>
#include <lib/string.h>
#include <stdint.h>

#define TTY_PROGRESS_BAR_WIDTH 30
#define TTY_PROGRESS_LINE_MAX  160

static char g_progress_title[96];
static char g_progress_step[96];
static int  g_progress_active = 0;

static void progress_set_text(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) return;

    if (!src) src = "";
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

static void line_append_str(char* buf, int* pos, int cap, const char* s) {
    if (!buf || !pos || cap <= 0 || !s) return;

    while (*s && *pos < cap - 1) {
        buf[*pos] = *s;
        (*pos)++;
        s++;
    }

    buf[*pos] = 0;
}

static void line_append_ch(char* buf, int* pos, int cap, char ch) {
    if (!buf || !pos || cap <= 0) return;
    if (*pos >= cap - 1) return;

    buf[*pos] = ch;
    (*pos)++;
    buf[*pos] = 0;
}

static void line_append_u32(char* buf, int* pos, int cap, uint32_t value) {
    char tmp[16];
    int n = 0;

    if (value == 0) {
        line_append_ch(buf, pos, cap, '0');
        return;
    }

    while (value > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (n > 0) {
        line_append_ch(buf, pos, cap, tmp[--n]);
    }
}

void tty_progress_begin(const char* title) {
    progress_set_text(g_progress_title, sizeof(g_progress_title), title);
    g_progress_step[0] = 0;
    g_progress_active = 1;

    printk("\n");
    if (g_progress_title[0]) {
        printk("%s\n", g_progress_title);
    }
}

void tty_progress_update(uint32_t current, uint32_t total) {
    if (!g_progress_active) return;

    if (total == 0) total = 1;
    if (current > total) current = total;

    uint32_t percent = (current * 100u) / total;
    uint32_t filled  = (percent * TTY_PROGRESS_BAR_WIDTH) / 100u;

    char line[TTY_PROGRESS_LINE_MAX];
    int pos = 0;
    memset(line, 0, sizeof(line));

    if (g_progress_step[0]) {
        line_append_str(line, &pos, sizeof(line), g_progress_step);
        line_append_ch(line, &pos, sizeof(line), ' ');
    }

    line_append_ch(line, &pos, sizeof(line), '[');

    for (uint32_t i = 0; i < TTY_PROGRESS_BAR_WIDTH; i++) {
        if (i < filled) line_append_ch(line, &pos, sizeof(line), '#');
        else            line_append_ch(line, &pos, sizeof(line), '.');
    }

    line_append_ch(line, &pos, sizeof(line), ']');
    line_append_ch(line, &pos, sizeof(line), ' ');

    line_append_u32(line, &pos, sizeof(line), percent);
    line_append_ch(line, &pos, sizeof(line), '%');

    // Eski satırdan karakter kalmaması için biraz padding
    line_append_ch(line, &pos, sizeof(line), ' ');
    line_append_ch(line, &pos, sizeof(line), ' ');
    line_append_ch(line, &pos, sizeof(line), ' ');
    line_append_ch(line, &pos, sizeof(line), ' ');
    line_append_ch(line, &pos, sizeof(line), ' ');

    printk("\r%s", line);
    fb_console_flush();
}

void tty_progress_step(const char* step, uint32_t current, uint32_t total) {
    progress_set_text(g_progress_step, sizeof(g_progress_step), step);
    tty_progress_update(current, total);
}

void tty_progress_end(void) {
    if (!g_progress_active) return;

    printk("\n");
    fb_console_flush();

    g_progress_active = 0;
    g_progress_title[0] = 0;
    g_progress_step[0] = 0;
}