#include <kernel/fs/vfs.h>
#include <kernel/user.h>         // USER_HOME_PATH / USER_DESKTOP_PATH
#include <ui/desktop_icons.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  const char* file;
  const char* title;
  int         app_id;   // ✅ ID ile çalışacağız
  const char* icon;
} sc_t;

static const sc_t k_defaults[] = {
  { "Terminal.ksf", "Terminal",      1, "/system/icons/terminal.kbi" },
  { "Files.ksf",    "File Manager",  2, "/system/icons/files.kbi"    },
  { "Notepad.ksf",  "Notepad",       3, "/system/icons/notepad.kbi"  },
  { "Demo.ksf",     "Demo",          5, "/system/icons/demo.kbi"     },

  // İstersen bunları da ekleyebilirsin (registry'ndeki id'lere göre):
  // { "Calculator.ksf", "Calculator", 6, "/system/icons/calc.kbi" },
  // { "Run.ksf",        "Run",        7, "/system/icons/run.kbi"  },
  // { "Scroll.ksf",     "Scroll Demo",11,"/system/icons/scroll.kbi"},
};

static int exists_file(const char* path) {
  vfs_stat_t st;
  if (!vfs_stat(path, &st)) return 0;
  return (st.type == VFS_T_FILE);
}

// RAMFS'te desktop dizinini garanti et (yazma fail olmasın)
static void ensure_desktop_dirs(void) {
  vfs_mkdir("/home");
#ifdef USER_HOME_PATH
  vfs_mkdir(USER_HOME_PATH);        // "/home/<user>"
#endif
  vfs_mkdir(USER_DESKTOP_PATH);     // "/home/<user>/desktop"
}

void desktop_seed_default_shortcuts(bool overwrite) {
  ensure_desktop_dirs();

  char path[160];

  for (int i = 0; i < (int)(sizeof(k_defaults)/sizeof(k_defaults[0])); i++) {
    const sc_t* s = &k_defaults[i];

    // path = USER_DESKTOP_PATH + "/" + file
    memset(path, 0, sizeof(path));
    strncpy(path, USER_DESKTOP_PATH, sizeof(path) - 1);
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, s->file, sizeof(path) - strlen(path) - 1);

    if (!overwrite && exists_file(path)) continue;
    if (overwrite && exists_file(path)) vfs_remove(path);

    // KSF content (ID ile)
    char ksf[256];
    memset(ksf, 0, sizeof(ksf));

    // Basit string build (snprintf yok)
    strcat(ksf, "title=");  strcat(ksf, s->title); strcat(ksf, "\n");

    // app_id=<n>
    strcat(ksf, "app_id=");
    {
      // int -> string (küçük helper)
      char num[16];
      int v = s->app_id;
      int p = 0;

      memset(num, 0, sizeof(num));
      if (v <= 0) {
        num[p++] = '0';
      } else {
        // ters yaz
        char tmp[16];
        int tp = 0;
        while (v > 0 && tp < (int)sizeof(tmp) - 1) {
          tmp[tp++] = (char)('0' + (v % 10));
          v /= 10;
        }
        // düz çevir
        while (tp > 0 && p < (int)sizeof(num) - 1) {
          num[p++] = tmp[--tp];
        }
      }
      num[p] = 0;
      strcat(ksf, num);
    }
    strcat(ksf, "\n");

    strcat(ksf, "icon=");   strcat(ksf, s->icon);  strcat(ksf, "\n");

    vfs_write_all(path, (const uint8_t*)ksf, (uint32_t)strlen(ksf));
  }

  // Yeniden tara + grid snap
  desktop_icons_init();
  desktop_icons_snap_all();
}