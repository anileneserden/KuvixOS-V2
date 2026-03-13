#include <kernel/user.h>
#include <lib/string.h>
#include <lib/commands.h>
#include <kernel/fs/vfs.h>
#include <stdbool.h>

static char g_active_username[32] = "anil"; // Varsayılan kullanıcı
static char g_active_password[32] = ""; 

// Küçük yardımcı: prefix match
static bool starts_with(const char* s, const char* pre) {
    if (!s || !pre) return false;
    while (*pre) {
        if (*s++ != *pre++) return false;
    }
    return true;
}

// Küçük yardımcı: güvenli append
static void str_append(char* out, int out_sz, const char* s) {
    if (!out || out_sz <= 0 || !s) return;
    int cur = (int)strlen(out);
    int i = 0;
    while (s[i] && (cur + i) < out_sz - 1) {
        out[cur + i] = s[i];
        i++;
    }
    out[cur + i] = '\0';
}

// ---------------------------------------------------------
// ✅ GÜNCELLENDİ: Satır Satır Kimlik Doğrulama (Linux Mantığı)
// ---------------------------------------------------------
bool user_authenticate(const char* username, const char* password) {
    // 1. Acil Durum / Fallback (Dosya sistemi bozulursa diye)
    if (strcmp(username, "anil") == 0 && strcmp(password, "123") == 0) {
        strncpy(g_active_username, "anil", 31);
        return true;
    }
    if (strcmp(username, "root") == 0 && strcmp(password, "root") == 0) {
        strncpy(g_active_username, "root", 31);
        return true;
    }

    // 2. Dosyadan Tarama
    uint8_t buf[512];
    uint32_t nread = 0;
    if (!vfs_read_all("/persist/system/config/users.cfg", buf, 511, &nread)) {
        return false;
    }
    buf[nread] = '\0';

    char* line = (char*)buf;
    while (line && *line) {
        // Bir sonraki satırın başlangıcını bul
        char* next_line = strchr(line, '\n');
        if (next_line) *next_line = '\0'; // Mevcut satırı geçici olarak sonlandır

        // Satırı ':' işaretine göre böl (user:pass)
        char* colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char* pass_part = colon + 1;

            // Satır sonu karakterlerini (\r) temizle
            int p_len = strlen(pass_part);
            if (p_len > 0 && pass_part[p_len-1] == '\r') pass_part[p_len-1] = '\0';

            // Eşleşme kontrolü
            if (strcmp(line, username) == 0 && strcmp(pass_part, password) == 0) {
                strncpy(g_active_username, username, 31);
                return true;
            }
        }

        // Bir sonraki satıra geç
        if (!next_line) break;
        line = next_line + 1;
    }

    return false;
}

// ---------------------------------------------------------
// ✅ GÜNCELLENDİ: users.cfg Temizliği ve Başlatma
// ---------------------------------------------------------
void user_init(void) {
    // KRİTİK: users.cfg dosyasındaki karmaşayı temizlemek için 
    // sistem her açıldığında varsayılanları yazar. 
    // (İstersen bir kez çalıştırdıktan sonra bu vfs_write_all satırını silebilirsin)
    const char* default_config = "root:root\nanil:123\nemre:123\n";
    vfs_write_all("/persist/system/config/users.cfg", (const uint8_t*)default_config, strlen(default_config));

    // Başlangıçta anil olarak oturum aç
    strncpy(g_active_username, "anil", 31);
}

const char* user_get_current_name(void) {
    return g_active_username;
}

// ---------------------------------------------------------
// ✅ GÜNCELLENDİ: Prompt Formatlama (Root için '#' işareti)
// ---------------------------------------------------------
void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    // Kullanıcı adı ve Host
    str_append(out, out_sz, g_active_username);
    str_append(out, out_sz, "@kuvixos:");

    // Yol formatlama (~/Desktop gibi)
    char pbuf[256];
    user_format_path(cwd_abs, pbuf, sizeof(pbuf), lang);
    str_append(out, out_sz, pbuf);

    // ✅ Linux Geleneği: Root ise '#', normal kullanıcı ise '$'
    if (strcmp(g_active_username, "root") == 0) {
        str_append(out, out_sz, "# ");
    } else {
        str_append(out, out_sz, "$ ");
    }
}

// Yol formatlama (Sabit kaldı)
void user_format_path(const char* abs_path, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';
    if (!abs_path || !abs_path[0]) {
        str_append(out, out_sz, "~");
        return;
    }
    if (starts_with(abs_path, USER_DESKTOP_PATH)) {
        str_append(out, out_sz, "~/");
        str_append(out, out_sz, (lang == USER_LANG_TR) ? "Masaustu" : "Desktop");
        const char* rest = abs_path + strlen(USER_DESKTOP_PATH);
        if (rest[0] == '/') str_append(out, out_sz, rest);
        return;
    }
    if (starts_with(abs_path, USER_HOME_PATH)) {
        str_append(out, out_sz, "~");
        const char* rest = abs_path + strlen(USER_HOME_PATH);
        if (rest[0]) str_append(out, out_sz, rest);
        return;
    }
    str_append(out, out_sz, abs_path);
}