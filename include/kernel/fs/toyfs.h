#pragma once
#include <stdint.h>
#include "../block/blockdev.h"

typedef int (*toyfs_iter_cb)(const char* path, uint32_t size, void* u);

int toyfs_mount(blockdev_t* dev);
int toyfs_open(const char* path);
int toyfs_read(int fd, void* buf, uint32_t n);
void toyfs_close(int fd);

// string liste (istersen kalsın)
int toyfs_list(const char* prefix, char* out, uint32_t out_sz);

// iterator (toyfs.c’de varsa bunun da prototipi burada olsun)
int toyfs_iter(const char* prefix, toyfs_iter_cb cb, void* u);
