// include/kernel/user.h
#pragma once

#ifndef CURRENT_USER
#define CURRENT_USER "anil"
#endif

#define USER_HOME_PATH    "/home/" CURRENT_USER
#define USER_DESKTOP_PATH "/home/" CURRENT_USER "/desktop"
#define USER_TRASH_PATH   "/home/" CURRENT_USER "/trash"
#define USER_APPS_PATH    "/home/" CURRENT_USER "/apps"   // ✅ EKLENECEK