#pragma once
int fs_init_once(void);
int fs_prepare_user_layout(void);
int fs_try_mount_removable_now(void);
void fs_detach_removable_now(void);