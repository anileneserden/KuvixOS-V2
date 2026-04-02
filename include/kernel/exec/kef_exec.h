#pragma once

#include <ui/apps/kef_minimal_app.h>

typedef enum {
	KEF_EXEC_FAILED = 0,
	KEF_EXEC_WINDOW_APP = 1,
	KEF_EXEC_CONSOLE_APP = 2,
} kef_exec_result_t;

kef_exec_result_t kef_exec_file(const char* path, kef_minimal_state_t* st);