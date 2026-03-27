#pragma once

#define SHELL_HISTORY_MAX 128
#define SHELL_CMD_MAX 128

void shell_history_init(void);
void shell_history_add(const char* cmd);

const char* shell_history_prev(void);
const char* shell_history_next(void);