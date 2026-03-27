#include <lib/shell/shell_history.h>
#include <lib/string.h>

static char history[SHELL_HISTORY_MAX][SHELL_CMD_MAX];
static int history_count = 0;
static int history_index = 0;

void shell_history_init(void) {
    history_count = 0;
    history_index = 0;
}

void shell_history_add(const char* cmd) {
    if (!cmd || cmd[0] == '\0') return;

    if (history_count > 0 && strcmp(history[history_count - 1], cmd) == 0) {
        history_index = history_count;
        return;
    }

    if (history_count < SHELL_HISTORY_MAX) {
        strcpy(history[history_count], cmd);
        history_count++;
    } else {
        for (int i = 1; i < SHELL_HISTORY_MAX; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[SHELL_HISTORY_MAX - 1], cmd);
    }

    history_index = history_count;
}

const char* shell_history_prev(void) {
    if (history_count == 0) return "";

    if (history_index > 0) {
        history_index--;
    }

    return history[history_index];
}

const char* shell_history_next(void) {
    if (history_count == 0) return "";

    if (history_index < history_count - 1) {
        history_index++;
        return history[history_index];
    }

    history_index = history_count;
    return "";
}