#pragma once

typedef void (*command_fn_t)(int argc, char** argv);

void commands_init(void);
void commands_execute(char* line);
