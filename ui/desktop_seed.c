// ui/desktop_seed.c

#include <kernel/fs/vfs.h>
#include <kernel/user.h>
#include <ui/desktop_icons.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------------------------------------
// helpers
// ------------------------------------------------------------
static void seed_write_if_missing(const char* path, const char* data) {
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 1 && st.type == VFS_T_FILE) {
        return; // dosya var -> dokunma
    }
    vfs_write_all(path, (const uint8_t*)data, (uint32_t)strlen(data));
}

static void kb_join2(char* out, int cap, const char* a, const char* b) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    if (!a) a = "";
    if (!b) b = "";

    strncpy(out, a, cap - 1);
    out[cap - 1] = 0;

    int n = (int)strlen(out);
    if (n > 0 && out[n - 1] != '/') {
        strncat(out, "/", (size_t)cap - strlen(out) - 1);
    }
    strncat(out, b, (size_t)cap - strlen(out) - 1);
}

static void get_desktop_path(char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;
    user_get_desktop_path(out, cap);
    if (!out[0]) {
        // safety fallback
        strncpy(out, "/home/anil/desktop", cap - 1);
        out[cap - 1] = 0;
    }
}

// ------------------------------------------------------------
// HTML seed
// ------------------------------------------------------------
void desktop_seed_html_pages(void) {
    char desk[256];
    get_desktop_path(desk, (int)sizeof(desk));

    // Masaüstü dizini yoksa oluştur
    vfs_mkdir(desk);

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

    char p1[256], p2[256], p3[256];
    kb_join2(p1, (int)sizeof(p1), desk, "home.html");
    kb_join2(p2, (int)sizeof(p2), desk, "docs.html");
    kb_join2(p3, (int)sizeof(p3), desk, "new.html");

    seed_write_if_missing(p1, home);
    seed_write_if_missing(p2, docs);
    seed_write_if_missing(p3, newtab);
}

// ------------------------------------------------------------
// hosts seed (/etc/hosts)
// home.local=/home/<user>/desktop/home.html
// ------------------------------------------------------------
void seed_hosts(void) {
    vfs_mkdir("/etc");

    vfs_stat_t st;
    if (vfs_stat("/etc/hosts", &st) == 1 && st.type == VFS_T_FILE) return;

    char desk[256];
    get_desktop_path(desk, (int)sizeof(desk));

    char hosts[512];
    hosts[0] = 0;

    strcat(hosts, "# KuvixOS hosts\n");

    strcat(hosts, "home.local=");
    strcat(hosts, desk);
    strcat(hosts, "/home.html\n");

    strcat(hosts, "docs.local=");
    strcat(hosts, desk);
    strcat(hosts, "/docs.html\n");

    strcat(hosts, "new.local=");
    strcat(hosts, desk);
    strcat(hosts, "/new.html\n");

    vfs_write_all("/etc/hosts", (const uint8_t*)hosts, (uint32_t)strlen(hosts));
}

// ------------------------------------------------------------
// vhosts seed (/etc/vhosts.conf)
// root runtime olduğu için conf string'i runtime build ediyoruz
// ------------------------------------------------------------
void seed_vhosts(void) {
    vfs_mkdir("/etc");

    char desk[256];
    get_desktop_path(desk, (int)sizeof(desk));

    // varsa dokunma
    vfs_stat_t st;
    if (vfs_stat("/etc/vhosts.conf", &st) == 1 && st.type == VFS_T_FILE) return;

    char conf[768];
    conf[0] = 0;

    strcat(conf, "# vhosts.conf (KuvixOS)\n");
    strcat(conf, "server {\n");
    strcat(conf, "  host home.local;\n");
    strcat(conf, "  root ");
    strcat(conf, desk);
    strcat(conf, ";\n");
    strcat(conf, "  index home.html;\n");
    strcat(conf, "}\n\n");

    strcat(conf, "server {\n");
    strcat(conf, "  host deneme.local;\n");
    strcat(conf, "  root /home/anil/web/deneme;\n");
    strcat(conf, "  index index.html;\n");
    strcat(conf, "  autoindex on;\n");
    strcat(conf, "}\n");

    vfs_write_all("/etc/vhosts.conf", (const uint8_t*)conf, (uint32_t)strlen(conf));
}

// ------------------------------------------------------------
// default shortcuts
// ------------------------------------------------------------
typedef struct {
    const char* file;
    const char* title;
    int         app_id;
    const char* icon;
} sc_t;

static const sc_t k_defaults[] = {
    { "Terminal.ksf",  "Terminal",       1, "/system/icons/terminal.kbi"  },
    { "Files.ksf",     "File Manager",   2, "/system/icons/files.kbi"     },
    { "Notepad.ksf",   "Notepad",        3, "/system/icons/notepad.kbi"   },
    { "Settings.ksf",  "Settings",      13, "/system/icons/settings.kbi"  },
    { "Browser.ksf",   "Kuvix Browser", 15, "/system/icons/browser.kbi"   },
    { "Controls.ksf",  "Controls",      17, "/system/icons/controls.kbi"  },
};

static int exists_file(const char* path) {
    vfs_stat_t st;
    if (!vfs_stat(path, &st)) return 0;
    return (st.type == VFS_T_FILE);
}

static void ensure_desktop_dirs(void) {
    char home[256];
    char desk[256];

    strncpy(home, user_get_home(), sizeof(home) - 1);
    home[sizeof(home) - 1] = 0;

    get_desktop_path(desk, (int)sizeof(desk));

    vfs_mkdir("/home");
    if (home[0]) vfs_mkdir(home);
    vfs_mkdir(desk);
}

void desktop_seed_default_shortcuts(bool overwrite) {
    ensure_desktop_dirs();

    char desk[256];
    get_desktop_path(desk, (int)sizeof(desk));

    char path[256];

    for (int i = 0; i < (int)(sizeof(k_defaults)/sizeof(k_defaults[0])); i++) {
        const sc_t* s = &k_defaults[i];

        // path = desktop + "/" + file
        kb_join2(path, (int)sizeof(path), desk, s->file);

        if (!overwrite && exists_file(path)) continue;
        if (overwrite && exists_file(path)) vfs_remove(path);

        char ksf[256];
        memset(ksf, 0, sizeof(ksf));

        strcat(ksf, "title=");  strcat(ksf, s->title); strcat(ksf, "\n");

        strcat(ksf, "app_id=");
        {
            char num[16];
            int v = s->app_id;
            int p = 0;

            memset(num, 0, sizeof(num));
            if (v <= 0) {
                num[p++] = '0';
            } else {
                char tmp[16];
                int tp = 0;
                while (v > 0 && tp < (int)sizeof(tmp) - 1) {
                    tmp[tp++] = (char)('0' + (v % 10));
                    v /= 10;
                }
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

    desktop_icons_init();
    desktop_icons_snap_all();
}