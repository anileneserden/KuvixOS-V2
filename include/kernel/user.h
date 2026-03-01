// include/kernel/user.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef CURRENT_USER
#define CURRENT_USER "anil"
#endif

#ifndef HOST
#define HOST "kuvixos"
#endif

// ✅ Gerçek path'ler (VFS için)
#define USER_HOME_PATH     "/home/" CURRENT_USER
#define USER_DESKTOP_PATH  USER_HOME_PATH "/desktop"
#define USER_TRASH_PATH    USER_HOME_PATH "/trash"
#define USER_APPS_PATH     USER_HOME_PATH "/apps"

// Terminal'de Desktop label (istersen TR/EN seçebilirsin)
typedef enum {
    USER_LANG_EN = 0,
    USER_LANG_TR = 1
} user_lang_t;

// ✅ "/home/anil/desktop" -> "~/Desktop" gibi dönüştürür
// out buffer'ına yazar, her zaman null-terminate eder.
void user_format_path(const char* abs_path, char* out, int out_sz, user_lang_t lang);

// ✅ Prompt üretmek için yardımcı (anil@kuvixos:~/Desktop> )
void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang);