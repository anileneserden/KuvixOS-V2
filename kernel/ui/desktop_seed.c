// kernel/ui/desktop_seed.c

#include <kernel/fs/vfs.h>
#include <kernel/user.h>         // USER_HOME_PATH / USER_DESKTOP_PATH
#include <ui/desktop_icons.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

static void seed_write_if_missing(const char* path, const char* data) {
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1 && st.type == VFS_T_FILE) {
        return; // dosya var -> dokunma
    }
    vfs_write_all(path, (const uint8_t*)data, (uint32_t)strlen(data));
}

void desktop_seed_html_pages(void) {
    // Masaüstü dizini yoksa oluştur (RAMFS için)
    vfs_mkdir(USER_DESKTOP_PATH);

    static const char* home =
        "<h1>KuvixOS HTML Viewer</h1>\n"
        "<p>Bu dosya boot'ta otomatik olusturuldu.</p>\n"
        "<p>Notepad ile acip duzenleyebilirsin.</p>\n"
        "<p>Turkce test: ğüşiöç İĞÜŞİÖÇ</p>\n"
        "<br>\n"
        "<p>Scroll test satirlari:</p>\n"
        "<p>Satir 1</p>\n"
        "<p>Satir 2</p>\n"
        "<p>Satir 3</p>\n"
        "<p>Satir 4</p>\n"
        "<p>Satir 5</p>\n"
        "<p>Satir 6</p>\n"
        "<p>Satir 7</p>\n"
        "<p>Satir 8</p>\n";

    static const char* docs =
        "<h1>Docs</h1>\n"
        "<p>local:docs sayfasi (masaustu dosyasi).</p>\n";

    static const char* newtab =
        "<h1>New</h1>\n"
        "<p>local:new sayfasi (masaustu dosyasi).</p>\n";

    // Path'leri masaüstüne yaz
    char p1[128], p2[128], p3[128];
    p1[0]=0; p2[0]=0; p3[0]=0;

    strncpy(p1, USER_DESKTOP_PATH, sizeof(p1)-1); p1[sizeof(p1)-1]=0;
    strncat(p1, "/home.html", sizeof(p1)-strlen(p1)-1);

    strncpy(p2, USER_DESKTOP_PATH, sizeof(p2)-1); p2[sizeof(p2)-1]=0;
    strncat(p2, "/docs.html", sizeof(p2)-strlen(p2)-1);

    strncpy(p3, USER_DESKTOP_PATH, sizeof(p3)-1); p3[sizeof(p3)-1]=0;
    strncat(p3, "/new.html", sizeof(p3)-strlen(p3)-1);

    seed_write_if_missing(p1, home);
    seed_write_if_missing(p2, docs);
    seed_write_if_missing(p3, newtab);
}

typedef struct {
  const char* file;
  const char* title;
  int         app_id;   // ✅ ID ile çalışacağız
  const char* icon;
} sc_t;

static const sc_t k_defaults[] = {
  { "Terminal.ksf", "Terminal",       1, "/system/icons/terminal.kbi"  },
  { "Files.ksf",    "File Manager",   2, "/system/icons/files.kbi"     },
  { "Notepad.ksf",  "Notepad",        3, "/system/icons/notepad.kbi"   },
  { "Settings.ksf", "Settings",      13, "/system/icons/settings.kbi"  },
  { "Browser.ksf",  "Kuvix Browser", 15, "/system/icons/browser.kbi"  },
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