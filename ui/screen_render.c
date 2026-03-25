#include <ui/screen.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/time.h>
#include <lib/string.h>

static void two_digit(char* out, int v) {
    out[0] = '0' + ((v / 10) % 10);
    out[1] = '0' + (v % 10);
}

static void four_digit(char* out, int v) {
    out[0] = '0' + ((v / 1000) % 10);
    out[1] = '0' + ((v / 100) % 10);
    out[2] = '0' + ((v / 10) % 10);
    out[3] = '0' + (v % 10);
}

static void format_time_string(char* out, int out_sz, const rtc_datetime_t* dt, const char* fmt) {
    (void)out_sz;

    if (!out || !dt) return;

    if (!fmt || !fmt[0] || strcmp(fmt, "HH:mm:ss") == 0) {
        /* HH:mm:ss */
        two_digit(out + 0, dt->hour);
        out[2] = ':';
        two_digit(out + 3, dt->min);
        out[5] = ':';
        two_digit(out + 6, dt->sec);
        out[8] = '\0';
        return;
    }

    if (!fmt || !fmt[0] || strcmp(fmt, "mm:ss") == 0) {
        /* mm:ss */
        two_digit(out + 0, dt->min);
        out[2] = ':';
        two_digit(out + 3, dt->sec);
        out[5] = '\0';
        return;
    }

    if (strcmp(fmt, "ss") == 0) {
        two_digit(out + 0, dt->sec);
        out[2] = '\0';
        return;
    }
    
    if (strcmp(fmt, "HH:mm") == 0) {
        /* HH:mm */
        two_digit(out + 0, dt->hour);
        out[2] = ':';
        two_digit(out + 3, dt->min);
        out[5] = '\0';
        return;
    }

    if (strcmp(fmt, "DD.MM.YYYY") == 0) {
        /* DD.MM.YYYY */
        two_digit(out + 0, dt->day);
        out[2] = '.';
        two_digit(out + 3, dt->month);
        out[5] = '.';
        four_digit(out + 6, dt->year);
        out[10] = '\0';
        return;
    }

    if (strcmp(fmt, "DD.MM.YYYY HH:mm") == 0) {
        /* DD.MM.YYYY HH:mm */
        two_digit(out + 0, dt->day);
        out[2] = '.';
        two_digit(out + 3, dt->month);
        out[5] = '.';
        four_digit(out + 6, dt->year);
        out[10] = ' ';
        two_digit(out + 11, dt->hour);
        out[13] = ':';
        two_digit(out + 14, dt->min);
        out[16] = '\0';
        return;
    }

    if (strcmp(fmt, "YYYY:MM:DD") == 0) {
        /* YYYY:MM:DD */
        four_digit(out + 0, dt->year);
        out[4] = ':';
        two_digit(out + 5, dt->month);
        out[7] = ':';
        two_digit(out + 8, dt->day);
        out[10] = '\0';
        return;
    }

    if (strcmp(fmt, "YYYY-MM-DD") == 0) {
        /* YYYY-MM-DD */
        four_digit(out + 0, dt->year);
        out[4] = '-';
        two_digit(out + 5, dt->month);
        out[7] = '-';
        two_digit(out + 8, dt->day);
        out[10] = '\0';
        return;
    }

    if (strcmp(fmt, "YYYY.MM.DD") == 0) {
        /* YYYY.MM.DD */
        four_digit(out + 0, dt->year);
        out[4] = '.';
        two_digit(out + 5, dt->month);
        out[7] = '.';
        two_digit(out + 8, dt->day);
        out[10] = '\0';
        return;
    }

    if (strcmp(fmt, "YYYY-MM-DD HH:mm:ss") == 0) {
        four_digit(out + 0, dt->year);
        out[4] = '-';
        two_digit(out + 5, dt->month);
        out[7] = '-';
        two_digit(out + 8, dt->day);
        out[10] = ' ';
        two_digit(out + 11, dt->hour);
        out[13] = ':';
        two_digit(out + 14, dt->min);
        out[16] = ':';
        two_digit(out + 17, dt->sec);
        out[19] = '\0';
        return;
    }

    /* fallback */
    two_digit(out + 0, dt->hour);
    out[2] = ':';
    two_digit(out + 3, dt->min);
    out[5] = ':';
    two_digit(out + 6, dt->sec);
    out[8] = '\0';
}

static void ui_render_panel(const ui_panel_t* p) {
    if (!p || !p->used || !p->visible) return;
    if (p->width <= 0 || p->height <= 0) return;

    gfx_fill_rect(p->x, p->y, p->width, p->height, p->background_color);
}

static void ui_render_label(const ui_label_t* l) {
    char buf[128];
    rtc_datetime_t now;

    if (!l || !l->used || !l->visible) return;

    buf[0] = '\0';

    if (l->bind[0]) {
        if (strcmp(l->bind, "time") == 0) {
            now = time_now_datetime_local();
            format_time_string(buf, sizeof(buf), &now, l->format);
        }
    }

    if (!buf[0] && l->text[0]) {
        strncpy(buf, l->text, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
    }

    if (!buf[0]) return;

    gfx_draw_text_utf8(l->x, l->y, l->color, buf);
}

void ui_screen_render(const ui_screen_t* screen) {
    if (!screen) return;

    fb_clear(screen->background_color);

    if (screen->panel.used) {
        ui_render_panel(&screen->panel);
    }

    if (screen->label.used) {
        ui_render_label(&screen->label);
    }
}