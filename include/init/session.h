#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>

typedef struct {
    uint32_t uid;
    uint32_t gid;
    char username[32];
    char home_dir[128];
} user_session_t;

void session_init(void);
user_session_t* session_get_current(void);
void session_set_user(uint32_t uid, const char* username, const char* home);
void session_handle_scancode(uint16_t scancode);
int authenticate_user(const char* username, const char* password);

#endif