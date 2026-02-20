#include <app/app.h>
#include <ui/wm.h>
#include <ui/messagebox.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <ui/controls/ui_context.h>
#include <ui/controls/panel2.h>
#include <ui/controls/button2.h>

#include <ui/apps/calculator.h>

#define CALC_MAX_INPUT 64

// ----------------------------
// Calculator state
// ----------------------------
struct calculator_t {
    int window_id;

    ui_context_t ui;
    ui_panel2_t  root;
    ui_button2_t btns[32];

    // giriş / hesap
    char input[CALC_MAX_INPUT];
    int  input_len;

    int64_t acc;
    char pending_op;   // 0, '+','-','*','/','%'

    int entering;      // 1: kullanıcı sayı yazıyor

    // display text (kalıcı buffer -> ???? olmaz)
    char display_text[64];
};

typedef struct calculator_t calculator_t;

// ----------------------------
// Helpers (int64)
// ----------------------------
static int64_t parse_i64(const char* s) {
    if (!s || !*s) return 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    int64_t v = 0;
    while (*s) {
        char ch = *s++;
        if (ch < '0' || ch > '9') break;
        v = v * 10 + (ch - '0');
    }
    return v * sign;
}

static void i64_to_str(int64_t v, char* out) {
    char tmp[32];
    int n = 0;
    if (v == 0) { out[0]='0'; out[1]=0; return; }
    if (v < 0) { *out++='-'; v = -v; }

    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

static void set_display(calculator_t* c, const char* s) {
    if (!s) s = "";
    int i = 0;
    for (; i < 63 && s[i]; i++) c->display_text[i] = s[i];
    c->display_text[i] = '\0';
}

static void reset_calc(calculator_t* c) {
    c->input[0] = 0;
    c->input_len = 0;
    c->acc = 0;
    c->pending_op = 0;
    c->entering = 0;
    set_display(c, "0");
}

static void commit_pending(calculator_t* c, int64_t rhs) {
    if (c->pending_op == 0) { c->acc = rhs; return; }

    switch (c->pending_op) {
        case '+': c->acc = c->acc + rhs; break;
        case '-': c->acc = c->acc - rhs; break;
        case '*': c->acc = c->acc * rhs; break;
        case '/':
            if (rhs == 0) { set_display(c, "ERR"); c->pending_op = 0; c->entering = 0; return; }
            c->acc = c->acc / rhs;
            break;
        case '%':
            if (rhs == 0) { set_display(c, "ERR"); c->pending_op = 0; c->entering = 0; return; }
            c->acc = c->acc % rhs;
            break;
        default: break;
    }
}

static void show_acc(calculator_t* c) {
    char buf[64];
    i64_to_str(c->acc, buf);
    set_display(c, buf);
}

static void push_digit(calculator_t* c, int d) {
    if (d < 0 || d > 9) return;
    if (c->input_len >= CALC_MAX_INPUT - 1) return;

    // Yeni sayı başlıyorsa input'u temizle
    if (!c->entering) {
        c->input_len = 0;
        c->input[0] = 0;
        c->entering = 1;
    }

    // leading zero kırp
    if (c->input_len == 1 && c->input[0] == '0') {
        c->input_len = 0;
        c->input[0] = 0;
    }

    c->input[c->input_len++] = (char)('0' + d);
    c->input[c->input_len] = 0;

    set_display(c, c->input);
}

static void toggle_sign(calculator_t* c) {
    if (!c->entering) {
        // acc üzerinde çalış
        c->acc = -c->acc;
        show_acc(c);
        return;
    }

    if (c->input_len <= 0) {
        c->input[0] = '0';
        c->input[1] = 0;
        c->input_len = 1;
    }

    if (c->input[0] == '-') {
        // remove '-'
        for (int i = 0; i < c->input_len; i++) c->input[i] = c->input[i+1];
        c->input_len--;
    } else {
        if (c->input_len < CALC_MAX_INPUT - 1) {
            // shift right
            for (int i = c->input_len; i >= 0; --i) c->input[i+1] = c->input[i];
            c->input[0] = '-';
            c->input_len++;
        }
    }
    set_display(c, c->input);
}

static void backspace(calculator_t* c) {
    if (!c->entering) return;
    if (c->input_len <= 0) return;

    c->input[--c->input_len] = 0;
    if (c->input_len == 0) {
        c->input[0] = '0';
        c->input[1] = 0;
        c->input_len = 1;
    }
    set_display(c, c->input);
}

static void clear_entry(calculator_t* c) {
    c->input_len = 0;
    c->input[0] = 0;
    c->entering = 0;
    set_display(c, "0");
}

static void clear_all(calculator_t* c) {
    reset_calc(c);
}

static void press_op(calculator_t* c, char op) {
    if (c->entering) {
        int64_t rhs = parse_i64(c->input);
        commit_pending(c, rhs);

        c->entering = 0;
        c->input_len = 0;
        c->input[0] = 0;
    } else {
        // entering değilse: sadece op değiştir
    }

    c->pending_op = op;

    // ✅ operatör feedback
    char opbuf[2] = { op, 0 };
    set_display(c, opbuf);
}

static void press_equals(calculator_t* c) {
    if (c->entering) {
        int64_t rhs = parse_i64(c->input);
        commit_pending(c, rhs);
        c->entering = 0;
        c->input_len = 0;
        c->input[0] = 0;
        c->pending_op = 0;
        show_acc(c);
    } else {
        // sadece acc göster
        show_acc(c);
    }
}

// ----------------------------
// Button mapping
// ----------------------------
typedef enum {
    ACT_NONE = 0,
    ACT_DIGIT,
    ACT_OP,
    ACT_EQ,
    ACT_CE,
    ACT_C,
    ACT_BKSP,
    ACT_SIGN
} act_kind_t;

typedef struct {
    calculator_t* calc;
    act_kind_t kind;
    int value; // digit veya op char
} btn_user_t;

static btn_user_t g_btn_user[32];

static void on_btn(void* user) {
    btn_user_t* u = (btn_user_t*)user;
    calculator_t* c = u->calc;

    switch (u->kind) {
        case ACT_DIGIT: push_digit(c, u->value); break;
        case ACT_OP:    press_op(c, (char)u->value); break;
        case ACT_EQ:    press_equals(c); break;
        case ACT_CE:    clear_entry(c); break;
        case ACT_C:     clear_all(c); break;
        case ACT_BKSP:  backspace(c); break;
        case ACT_SIGN:  toggle_sign(c); break;
        default: break;
    }

    // UI redraw için
    c->ui.has_dirty = true;
}

// ----------------------------
// Layout + Display draw (foto gibi)
// ----------------------------
static int text_len8(const char* s) {
    int n = 0; if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void layout(calculator_t* c, ui_rect_t cr) {
    c->root.base.location = (ui_point_t){0,0};
    c->root.base.size     = (ui_size_t){cr.w, cr.h};

    // Display alanı yüksekliği (foto gibi üstte büyük)
    // İstersen büyüt: 70-90 arası
    (void)c;
}

static void draw_display(const calculator_t* c, ui_rect_t cr) {
    // Display bölgesi
    int pad = 10;
    int disp_h = 70;
    int x = 0;
    int y = 0;
    int w = cr.w;
    int h = disp_h;

    // Arka plan + alt çizgi
    gfx_fill_rect(x, y, w, h, 0xFFFFFF);
    gfx_draw_line(x, y + h - 1, x + w, y + h - 1, 0xD0D0D0);

    // Sağ hizalı text (font 8px geniş)
    const char* s = c->display_text[0] ? c->display_text : "0";
    int len = text_len8(s);
    int tw = len * 8;

    int tx = x + w - pad - tw;
    if (tx < x + pad) tx = x + pad;

    // Dikey ortalama (font 16px)
    int ty = y + (h - 16) / 2;

    gfx_draw_text_utf8(tx, ty, 0x000000, s);
}

static void layout_buttons(calculator_t* c, ui_rect_t cr) {
    // Display altı grid
    int disp_h = 70;
    int margin = 10;
    int gap = 8;

    int grid_x = margin;
    int grid_y = disp_h + margin;
    int grid_w = cr.w - margin * 2;
    int grid_h = cr.h - grid_y - margin;
    if (grid_h < 0) grid_h = 0;

    int cols = 4;
    int rows = 5;

    int bw = (grid_w - gap * (cols - 1)) / cols;
    int bh = (grid_h - gap * (rows - 1)) / rows;

    // Dikdörtgen hissi için min height clamp
    if (bh > 42) bh = 42;
    if (bh < 26) bh = 26;

    // 5x4 = 20 buton
    for (int i = 0; i < 20; i++) {
        int r = i / 4;
        int col = i % 4;
        int x = grid_x + col * (bw + gap);
        int y = grid_y + r * (bh + gap);

        c->btns[i].base.location = (ui_point_t){x, y};
        c->btns[i].base.size     = (ui_size_t){bw, bh};
        c->btns[i].base.visible  = true;
        c->btns[i].base.enabled  = true;
    }
}

// ----------------------------
// App callbacks
// ----------------------------
static calculator_t g_calc;

static void calculator_on_create(app_t* self) {
    self->user = &g_calc;

    calculator_t* c = (calculator_t*)self->user;
    c->window_id = self->win_id;

    ui_ctx_init(&c->ui);

    ui_panel2_init(&c->root, 1, (ui_point_t){0,0}, (ui_size_t){0,0}, 0xFFFFFF);
    ui_panel2_set_border(&c->root, 1, 0xC0C0C0);

    // Windows dizilimi (MVP)
    // Row1: %  CE  C  ⌫
    // Row2: 7  8   9  ÷
    // Row3: 4  5   6  ×
    // Row4: 1  2   3  −
    // Row5: ±  0   .  =
    //
    // Not: '.' şimdilik sadece UI olsun (fixed-point eklemedik), istersen disable ederiz.
    const char* labels[20] = {
        "%","CE","C","BK",
        "7","8","9","/",
        "4","5","6","×",
        "1","2","3","-",
        "±","0",".","="
    };

    for (int i = 0; i < 20; i++) {
        ui_button2_init(&c->btns[i], 100 + i, (ui_point_t){0,0}, (ui_size_t){0,0}, labels[i]);
        ui_control_add_child(&c->root.base, &c->btns[i].base);

        g_btn_user[i].calc = c;
        g_btn_user[i].kind = ACT_NONE;
        g_btn_user[i].value = 0;

        const char* t = labels[i];

        // digits
        if (t[0] >= '0' && t[0] <= '9' && t[1] == 0) {
            g_btn_user[i].kind = ACT_DIGIT;
            g_btn_user[i].value = t[0] - '0';
        }
        // ops
        else if (t[0] == '+' && t[1] == 0) { g_btn_user[i].kind = ACT_OP; g_btn_user[i].value = '+'; }
        else if (t[0] == '-' && t[1] == 0) { g_btn_user[i].kind = ACT_OP; g_btn_user[i].value = '-'; }
        else if (t[0] == '*' && t[1] == 0) { g_btn_user[i].kind = ACT_OP; g_btn_user[i].value = '*'; }
        else if (t[0] == '/' && t[1] == 0) { g_btn_user[i].kind = ACT_OP; g_btn_user[i].value = '/'; }
        else if (t[0] == '%' && t[1] == 0) { g_btn_user[i].kind = ACT_OP; g_btn_user[i].value = '%'; }

        else if (t[0] == '=' && t[1] == 0) { g_btn_user[i].kind = ACT_EQ; }
        else if (t[0] == 'C' && t[1] == 0) { g_btn_user[i].kind = ACT_C; }
        else if (t[0] == 'C' && t[1] == 'E' && t[2] == 0) { g_btn_user[i].kind = ACT_CE; }

        // BK (backspace)
        else if (t[0] == 'B' && t[1] == 'K' && t[2] == 0) { g_btn_user[i].kind = ACT_BKSP; }

        // +/- (sign)
        else if (t[0] == '+' && t[1] == '/' && t[2] == '-' && t[3] == 0) { g_btn_user[i].kind = ACT_SIGN; }

        // '.' (şimdilik disable ediyorsan bırak)
        else if (t[0] == '.' && t[1] == 0) {
            c->btns[i].base.enabled = false;  // fixed-point ekleyince açarız
        }

        ui_button2_onclick(&c->btns[i], on_btn, &g_btn_user[i]);
    }

    ui_ctx_add_root(&c->ui, &c->root.base);

    reset_calc(c);
}

static void calculator_on_draw(app_t* self) {
    calculator_t* c = (calculator_t*)self->user;

    ui_rect_t cr = wm_get_client_rect(c->window_id);

    // Arka plan
    gfx_fill_rect(0, 0, cr.w, cr.h, 0xFFFFFF);

    // Display
    draw_display(c, cr);

    // Buttons layout
    layout_buttons(c, cr);

    // Controls draw
    ui_ctx_draw(&c->ui);
}

static void calculator_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t pressed, uint8_t released) {
    (void)pressed; (void)released;
    calculator_t* c = (calculator_t*)self->user;

    if (messagebox_is_visible()) return;
    if (wm_is_any_window_captured()) return;

    ui_rect_t cr = wm_get_client_rect(c->window_id);
    int lx = mx - cr.x;
    int ly = my - cr.y;

    bool ldown = (buttons & 1) != 0;
    ui_ctx_mouse(&c->ui, lx, ly, ldown);
}

static void calculator_on_key(app_t* self, uint16_t sc) { (void)self; (void)sc; }
static void calculator_on_destroy(app_t* self) { (void)self; }

const app_vtbl_t calculator_vtbl = {
    .on_create = calculator_on_create,
    .on_draw   = calculator_on_draw,
    .on_key    = calculator_on_key,
    .on_mouse  = calculator_on_mouse,
    .on_destroy= calculator_on_destroy,
    .on_close_request = 0
};
