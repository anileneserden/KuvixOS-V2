#pragma once

#include <stdint.h>
#include <kernel/user.h>
#include <app/app.h>

extern const app_vtbl_t terminal_vtabl;

#ifdef __cplusplus
extern "C" {
#endif

// Terminal line buffer
#define TERM_MAX_LINES   2048
#define TERM_LINE_MAX    256
#define TERM_INPUT_MAX   256

typedef struct {
    char text[TERM_LINE_MAX];
    int  len;
} term_line_t;

typedef struct {
    // input line
    char input[TERM_INPUT_MAX];
    int  in_len;

    // scrollback lines
    term_line_t lines[TERM_MAX_LINES];
    int  line_count;     // how many valid lines (<= TERM_MAX_LINES)
    int  view_start;     // first visible line index (0..line_count-1)

    // rendering params (cached)
    int cols;            // chars per line (client.w / 8)
    int rows;            // visible rows (client.h / line_h)
    int line_h;          // 14 or 16 (senin font 16 ama spacing 14 kullanıyorsun)

    // cwd / lang for prompt
    char cwd[128];
    user_lang_t lang;
    int suppress_next_prompt;
} terminal_t;

#ifdef __cplusplus
}
#endif