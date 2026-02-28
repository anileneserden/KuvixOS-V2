#include <kernel/fs/vfs.h>
#include <kernel/user.h>
#include <ui/desktop_icons.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  const char* file;
  const char* title;
  int         app_id;     // built-in için (0 ise kullanılmaz)
  const char* kef_path;   // KEF için (NULL ise kullanılmaz)
  const char* icon;
} sc_t;

/*
  app_id > 0 → built-in
  kef_path != NULL → KEF uygulaması
*/
static const sc_t k_defaults[] = {
  { "Terminal.ksf", "Terminal",      1,  NULL, "/system/icons/terminal.kbi" },
  { "Files.ksf",    "File Manager",  2,  NULL, "/system/icons/files.kbi"    },
  { "Notepad.ksf",  "Notepad",       3,  NULL, "/system/icons/notepad.kbi"  },
  { "Settings.ksf", "Settings",     13,  NULL, "/system/icons/settings.kbi" },

  // ÖRNEK KEF UYGULAMASI
  { "Hello.ksf",    "Hello KEF",     0, "/home/anil/apps/hello.kef", "/system/icons/app.kbi" }
};

static int exists_file(const char* path) {
  vfs_stat_t st;
  if (!vfs_stat(path, &st)) return 0;
  return (st.type == VFS_T_FILE);
}

static void ensure_desktop_dirs(void) {
  vfs_mkdir("/home");
#ifdef USER_HOME_PATH
  vfs_mkdir(USER_HOME_PATH);
#endif
  vfs_mkdir(USER_DESKTOP_PATH);
}

static void append_int(char* dst, int value) {
  char tmp[16];
  char buf[16];
  int tp = 0;
  int p = 0;

  memset(tmp, 0, sizeof(tmp));
  memset(buf, 0, sizeof(buf));

  if (value <= 0) {
    buf[p++] = '0';
  } else {
    while (value > 0 && tp < (int)sizeof(tmp) - 1) {
      tmp[tp++] = (char)('0' + (value % 10));
      value /= 10;
    }
    while (tp > 0 && p < (int)sizeof(buf) - 1) {
      buf[p++] = tmp[--tp];
    }
  }

  strcat(dst, buf);
}

void desktop_seed_default_shortcuts(bool overwrite) {
  ensure_desktop_dirs();

  char path[160];

  for (int i = 0; i < (int)(sizeof(k_defaults)/sizeof(k_defaults[0])); i++) {

    const sc_t* s = &k_defaults[i];

    memset(path, 0, sizeof(path));
    strncpy(path, USER_DESKTOP_PATH, sizeof(path) - 1);
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, s->file, sizeof(path) - strlen(path) - 1);

    if (!overwrite && exists_file(path)) continue;
    if (overwrite && exists_file(path)) vfs_remove(path);

    char ksf[256];
    memset(ksf, 0, sizeof(ksf));

    // title
    strcat(ksf, "title=");
    strcat(ksf, s->title);
    strcat(ksf, "\n");

    // built-in app
    if (s->app_id > 0) {
      strcat(ksf, "app_id=");
      append_int(ksf, s->app_id);
      strcat(ksf, "\n");
    }

    // KEF app
    if (s->kef_path) {
      strcat(ksf, "kef=");
      strcat(ksf, s->kef_path);
      strcat(ksf, "\n");
    }

    // icon
    if (s->icon) {
      strcat(ksf, "icon=");
      strcat(ksf, s->icon);
      strcat(ksf, "\n");
    }

    vfs_write_all(path, (const uint8_t*)ksf, (uint32_t)strlen(ksf));
  }

  desktop_icons_init();
  desktop_icons_snap_all();
}