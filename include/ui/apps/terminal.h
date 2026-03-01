// terminal.h

#include <kernel/user.h>

#define TERM_MAX_LINES   2048
#define TERM_LINE_MAX    256
#define TERM_INPUT_MAX   256

typedef struct {
    char text[TERM_LINE_MAX];
    int  len;
} term_line_t;

typedef struct {
    char input[TERM_INPUT_MAX];
    int  in_len;

    int  line_count;
    int  view_start;

    int cols;
    int rows;
    int line_h;

    char cwd[128];
    user_lang_t lang;
    int suppress_next_prompt;
} terminal_t;