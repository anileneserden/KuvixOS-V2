#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <kernel/fs/vfs.h>

#define MAX_BACKGROUND_TASKS 16

typedef struct {
    uint32_t id;
    int active;
    uint8_t* buffer;      // Sürecin bellek alanı (kmalloc ile ayrılan)
    char path[VFS_PATH_MAX];
} bg_task_t;

extern bg_task_t g_bg_tasks[MAX_BACKGROUND_TASKS];
extern uint32_t g_next_task_id;

#endif