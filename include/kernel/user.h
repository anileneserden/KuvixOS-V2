// include/kernel/user.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    USER_LANG_EN = 0,
    USER_LANG_TR = 1
} user_lang_t;

typedef struct user_profile {
    char username[32];
    char hostname[32];
    char home[128];
} user_profile_t;

void user_set_defaults(void);
bool user_load(const char* path);

const char* user_get_username(void);
const char* user_get_hostname(void);
const char* user_get_home(void);

void user_get_desktop_path(char* out, int out_sz);
void user_get_trash_path(char* out, int out_sz);
void user_get_apps_path(char* out, int out_sz);

void user_format_path(const char* abs_path, char* out, int out_sz, user_lang_t lang);
void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang);