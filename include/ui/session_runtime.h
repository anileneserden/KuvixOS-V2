#pragma once

int session_run_path(const char* desktop_json_path);
int session_kill_active(void);
int session_autorun_from_config(void);

const char* session_active_path(void);
int session_is_shell_running(void);