#include <kernel/user.h>
#include <lib/string.h>
#include <lib/commands.h>
#include <kernel/fs/vfs.h>
#include <stdbool.h>

static char g_active_username[32] = "guest";
static char g_active_password[32] = ""; // Diskten okunan şifreyi burada tutacağız

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
// ✅ YENİ: Login Doğrulama Fonksiyonu
// ---------------------------------------------------------
bool user_authenticate(const char* username, const char* password) {
    // Eğer diskten okunan kullanıcı ve şifreyle eşleşiyorsa true dön
    if (strcmp(username, g_active_username) == 0 && 
        strcmp(password, g_active_password) == 0) {
        return true;
    }
    
    // Güvenlik için: anil/123 her zaman çalışsın (acil durum kapısı)
    if (strcmp(username, "anil") == 0 && strcmp(password, "123") == 0) {
        return true;
    }

    return false;
}

// ---------------------------------------------------------
// ✅ GÜNCELLENDİ: user_init artık şifreyi de hafızaya alıyor
// ---------------------------------------------------------
void user_init(void) {
    char buf[64];
    uint32_t nread = 0;

    // "/persist/system/config/users.cfg" dosyasını oku
    int success = vfs_read_all("/persist/system/config/users.cfg", (uint8_t*)buf, 63, &nread);

    if (success && nread > 0) {
        buf[nread] = '\0';
        
        // "anil:1234" gibi bir içerikten ismi ve şifreyi ayıklayalım
        char* colon = strchr(buf, ':');
        if (colon) {
            *colon = '\0'; // ':' yerine NULL koyarak stringi ikiye bölüyoruz
            char* pass_part = colon + 1;

            // Sondaki olası \n veya \r karakterlerini temizle
            for(int i = 0; pass_part[i]; i++) {
                if(pass_part[i] == '\n' || pass_part[i] == '\r') pass_part[i] = '\0';
            }

            strncpy(g_active_username, buf, 31);
            strncpy(g_active_password, pass_part, 31);
        } else {
            // Dosyada ':' yoksa sadece kullanıcı adı var kabul et
            strncpy(g_active_username, buf, 31);
            strncpy(g_active_password, "", 31); // Şifre boş
        }
    } else {
        // Dosya yoksa varsayılanlar
        strncpy(g_active_username, "anil", 31);
        strncpy(g_active_password, "123", 31);
    }
}

const char* user_get_current_name(void) {
    return g_active_username;
}

// Yol formatlama (Aynen kaldı)
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

// Prompt formatlama (Aynen kaldı)
void user_format_prompt(const char* cwd_abs, char* out, int out_sz, user_lang_t lang) {
    if (!out || out_sz <= 0) return;
    out[0] = '\0';
    str_append(out, out_sz, g_active_username);
    str_append(out, out_sz, "@");
    str_append(out, out_sz, "kuvix"); // HOST yerine doğrudan yazdım
    str_append(out, out_sz, ":");
    char pbuf[256];
    user_format_path(cwd_abs, pbuf, sizeof(pbuf), lang);
    str_append(out, out_sz, pbuf);
}