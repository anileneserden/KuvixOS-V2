#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void memmon_toggle(void);
bool memmon_is_visible(void);

// Desktop draw/tick içinden çağıracağız
void memmon_draw(int client_w, int client_h);

#ifdef __cplusplus
}
#endif