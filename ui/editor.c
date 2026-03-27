#include <ui/editor.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <stdint.h>

#define EDITOR_BUFFER_SIZE 4096
#define EDITOR_PATH_SIZE   128

#define COLOR_TEXT_FG   0x00FFFFFF
#define COLOR_TEXT_BG   0x00000000
#define COLOR_BAR_FG    0x00000000
#define COLOR_BAR_BG    0x00C0C0C0
#define COLOR_STATUS_FG 0x00000000
#define COLOR_STATUS_BG 0x00C0C0C0
#define COLOR_CURSOR_FG 0x00000000
#define COLOR_CURSOR_BG 0x0000FF00

#define EDITOR_TOP_ROW        0
#define EDITOR_TEXT_START_ROW 1

static char g_editor_path[EDITOR_PATH_SIZE];
static char g_editor_buffer[EDITOR_BUFFER_SIZE];
static int  g_editor_len = 0;
static int  g_editor_active = 0;

static int  g_editor_needs_redraw = 0;
static int  g_editor_modified = 0;

// append-only cursor position
static int  g_text_row = 0;
static int  g_text_col = 0;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void editor_reset(void) {
    memset(g_editor_path, 0, sizeof(g_editor_path));
    memset(g_editor_buffer, 0, sizeof(g_editor_buffer));

    g_editor_len = 0;
    g_editor_active = 0;
    g_editor_needs_redraw = 0;
    g_editor_modified = 0;
    g_text_row = 0;
    g_text_col = 0;
}

static void editor_mark_redraw(void) {
    g_editor_needs_redraw = 1;
}

static int editor_footer_row_1(void) {
    int rows = fb_console_rows();
    return (rows > 2) ? (rows - 2) : 0;
}

static int editor_footer_row_2(void) {
    int rows = fb_console_rows();
    return (rows > 1) ? (rows - 1) : 0;
}

static void editor_recompute_cursor_from_buffer(void) {
    g_text_row = 0;
    g_text_col = 0;

    for (int i = 0; i < g_editor_len; i++) {
        char c = g_editor_buffer[i];
        if (c == '\n') {
            g_text_row++;
            g_text_col = 0;
        } else {
            g_text_col++;
        }
    }
}

static const char* editor_basename(const char* path) {
    if (!path || !path[0]) return "(isimsiz)";

    const char* last = path;
    for (const char* p = path; *p; ++p) {
        if (*p == '/') last = p + 1;
    }

    return (*last) ? last : path;
}

// ------------------------------------------------------------
// Drawing
// ------------------------------------------------------------
static void editor_draw_top_bar(void) {
    int cols = fb_console_cols();
    const char* left = "Editor 0.0.1";
    const char* name = editor_basename(g_editor_path);

    fb_console_set_color(COLOR_BAR_FG, COLOR_BAR_BG);
    fb_console_set_cursor(0, EDITOR_TOP_ROW);

    for (int i = 0; i < cols; i++) {
        printk(" ");
    }

    fb_console_set_cursor(0, EDITOR_TOP_ROW);
    printk("%s", left);

    int name_len = (int)strlen(name);
    int name_col = (cols - name_len) / 2;
    if (name_col < 0) name_col = 0;

    fb_console_set_cursor(name_col, EDITOR_TOP_ROW);
    printk("%s", name);

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
}

static void editor_draw_footer(void) {
    int cols = fb_console_cols();
    int row1 = editor_footer_row_1();
    int row2 = editor_footer_row_2();

    const char* status = 0;
    if (g_editor_modified) status = "[ Modified ]";
    else if (g_editor_len == 0) status = "[ New File ]";
    else status = "[ Ready ]";

    int status_len = (int)strlen(status);
    int status_col = (cols - status_len) / 2;
    if (status_col < 0) status_col = 0;

    fb_console_set_color(COLOR_STATUS_FG, COLOR_STATUS_BG);

    fb_console_set_cursor(0, row1);
    for (int i = 0; i < cols; i++) {
        printk(" ");
    }
    fb_console_set_cursor(status_col, row1);
    printk("%s", status);

    fb_console_set_cursor(0, row2);
    for (int i = 0; i < cols; i++) {
        printk(" ");
    }
    fb_console_set_cursor(0, row2);
    printk("^X Exit   ^O Write Out");

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
}

static void editor_draw_text_area(void) {
    int row = EDITOR_TEXT_START_ROW;
    int col = 0;
    int footer1 = editor_footer_row_1();

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);

    for (int i = 0; i < g_editor_len; i++) {
        char c = g_editor_buffer[i];

        if (row >= footer1) break;

        if (c == '\n') {
            row++;
            col = 0;
            continue;
        }

        fb_console_set_cursor(col, row);
        printk("%c", (unsigned char)c);
        col++;
    }
}

static void editor_draw_cursor(void) {
    int draw_row = EDITOR_TEXT_START_ROW + g_text_row;
    int draw_col = g_text_col;
    int footer1 = editor_footer_row_1();

    if (draw_row >= footer1) return;

    fb_console_set_color(COLOR_CURSOR_FG, COLOR_CURSOR_BG);
    fb_console_set_cursor(draw_col, draw_row);
    printk(" ");
    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
    fb_console_flush();
}

static void editor_draw_full(void) {
    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
    fb_console_clear();

    editor_draw_top_bar();
    editor_draw_text_area();
    editor_draw_footer();
    editor_draw_cursor();

    fb_console_flush();
    g_editor_needs_redraw = 0;
}

static void editor_draw_append_char(char c) {
    int draw_row = EDITOR_TEXT_START_ROW + g_text_row;
    int draw_col = g_text_col;
    int footer1 = editor_footer_row_1();

    if (draw_row >= footer1) return;
    if (c == '\n') return;

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
    fb_console_set_cursor(draw_col, draw_row);
    printk("%c", (unsigned char)c);
    fb_console_flush();
}

static void editor_clear_line_visual(int text_row) {
    int footer1 = editor_footer_row_1();
    int draw_row = EDITOR_TEXT_START_ROW + text_row;
    int cols = fb_console_cols();

    if (draw_row >= footer1) return;

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
    fb_console_set_cursor(0, draw_row);

    for (int i = 0; i < cols; i++) {
        printk(" ");
    }
}

static void editor_draw_line_visual(int target_row) {
    int footer1 = editor_footer_row_1();
    int draw_row = EDITOR_TEXT_START_ROW + target_row;

    if (draw_row >= footer1) return;

    fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
    fb_console_set_cursor(0, draw_row);

    int row = 0;
    int col = 0;

    for (int i = 0; i < g_editor_len; i++) {
        char c = g_editor_buffer[i];

        if (c == '\n') {
            if (row == target_row) {
                break;
            }
            row++;
            col = 0;
            continue;
        }

        if (row == target_row) {
            fb_console_set_cursor(col, draw_row);
            printk("%c", (unsigned char)c);
            col++;
        }
    }
}

static void editor_redraw_current_line(void) {
    editor_clear_line_visual(g_text_row);
    editor_draw_line_visual(g_text_row);
    editor_draw_cursor();
    fb_console_flush();
}

static void editor_redraw_footer_only(void) {
    editor_draw_footer();
    fb_console_flush();
}

// ------------------------------------------------------------
// File I/O
// ------------------------------------------------------------
static void editor_load_file(void) {
    uint32_t out_size = 0;

    g_editor_len = 0;
    g_editor_buffer[0] = '\0';

    int rc = vfs_read_all(
        g_editor_path,
        (uint8_t*)g_editor_buffer,
        (uint32_t)(EDITOR_BUFFER_SIZE - 1),
        &out_size
    );

    if (rc == 0) {
        g_editor_len = (int)out_size;
        if (g_editor_len < 0) g_editor_len = 0;
        if (g_editor_len >= EDITOR_BUFFER_SIZE) g_editor_len = EDITOR_BUFFER_SIZE - 1;
        g_editor_buffer[g_editor_len] = '\0';
    } else {
        g_editor_len = 0;
        g_editor_buffer[0] = '\0';
    }

    g_editor_modified = 0;
    editor_recompute_cursor_from_buffer();
    editor_mark_redraw();
}

static void editor_save_file(void) {
    int rc = vfs_write_all(
        g_editor_path,
        (const uint8_t*)g_editor_buffer,
        (uint32_t)g_editor_len
    );

    if (rc == 0) {
        g_editor_modified = 0;
        editor_mark_redraw();
    } else {
        fb_console_set_cursor(0, editor_footer_row_1());
        fb_console_set_color(COLOR_TEXT_FG, COLOR_TEXT_BG);
        printk("[editor] kaydetme hatasi: %s", g_editor_path);
        fb_console_flush();
    }
}

// ------------------------------------------------------------
// Editing helpers
// ------------------------------------------------------------
static void editor_insert_char(char c) {
    if (g_editor_len >= EDITOR_BUFFER_SIZE - 1) return;

    int was_modified = g_editor_modified;

    g_editor_buffer[g_editor_len++] = c;
    g_editor_buffer[g_editor_len] = '\0';
    g_editor_modified = 1;

    if (c == '\n') {
        g_text_row++;
        g_text_col = 0;
        editor_mark_redraw();
    } else {
        editor_draw_append_char(c);
        g_text_col++;
        editor_draw_cursor();
    }

    if (!was_modified) {
        editor_redraw_footer_only();
        editor_draw_cursor();
        fb_console_flush();
    }
}

static void editor_backspace(void) {
    if (g_editor_len <= 0) return;

    int was_modified = g_editor_modified;
    char removed = g_editor_buffer[g_editor_len - 1];

    g_editor_len--;
    g_editor_buffer[g_editor_len] = '\0';
    g_editor_modified = 1;

    if (removed == '\n') {
        editor_recompute_cursor_from_buffer();
        editor_mark_redraw();
        return;
    }

    if (g_text_col > 0) {
        g_text_col--;
        editor_redraw_current_line();
    } else {
        editor_recompute_cursor_from_buffer();
        editor_mark_redraw();
        return;
    }

    if (!was_modified) {
        editor_redraw_footer_only();
        editor_draw_cursor();
        fb_console_flush();
    }
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void editor_open(const char* path) {
    editor_reset();

    if (path) {
        strncpy(g_editor_path, path, EDITOR_PATH_SIZE - 1);
        g_editor_path[EDITOR_PATH_SIZE - 1] = '\0';
    }

    g_editor_active = 1;
    editor_load_file();
}

void editor_init(void) {
    editor_reset();
}

void editor_tick(void) {
    if (!g_editor_active) return;

    if (g_editor_needs_redraw) {
        editor_draw_full();
    }
}

int editor_is_active(void) {
    return g_editor_active;
}

void editor_handle_key(uint16_t key) {
    if (!g_editor_active) return;

    if ((key & 0xFF00) == 0xFF00) {
        return;
    }

    char c = (char)key;

    // Ctrl+X veya Ctrl+Q
    if ((uint8_t)c == 24 || (uint8_t)c == 17) {
        g_editor_active = 0;
        fb_console_clear();
        fb_console_flush();
        return;
    }

    // Ctrl+O veya Ctrl+S
    if ((uint8_t)c == 15 || (uint8_t)c == 19) {
        editor_save_file();
        return;
    }

    // Backspace
    if (c == '\b' || (uint8_t)c == 8 || (uint8_t)c == 127) {
        editor_backspace();
        return;
    }

    // Enter
    if (c == '\r' || c == '\n') {
        editor_insert_char('\n');
        return;
    }

    // Printable
    if ((uint8_t)c >= 32) {
        editor_insert_char(c);
        return;
    }
}