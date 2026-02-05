#include <ui/desktop_icons.h>
#include <ui/desktop_icons/text_file.h>
#include <ui/desktop_icons/generic_file.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <app/app_manager.h>
#include <ui/desktop_icons/folder_icon.h>

// Sürücündeki global fare değişkenlerini tanıtıyoruz
extern int mouse_x;
extern int mouse_y;

#define MAX_DESKTOP_ICONS 32
static desktop_icon_t icons[MAX_DESKTOP_ICONS];
static int icon_count = 0;

static bool snap_to_grid = true; // Varsayılan olarak açık olsun

// Yardımcı uzantı kontrolü
static bool ends_with(const char* str, const char* suffix) {
    int str_len = strlen(str);
    int suffix_len = strlen(suffix);
    if (str_len < suffix_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// VFS'den dosya okuma callback fonksiyonu
static int desktop_load_callback(const char* path, uint32_t size, void* u) {
    (void)size; (void)u;
    if (icon_count >= MAX_DESKTOP_ICONS) return 0;

    // 1. Kendi dizinini filtrele (Masaüstünün içinde masaüstünü görmeyelim)
    if (strcmp(path, "/home") == 0 || strcmp(path, "/home/desktop") == 0 || 
        strcmp(path, "/home/desktop/") == 0 || strcmp(path, "/") == 0) {
        return 1; 
    }

    // 2. Dosya ismini ayıkla (Örn: /home/desktop/deneme -> deneme)
    const char* filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;

    // Gizli dosyaları ele
    if (filename[0] == '\0' || filename[0] == '.') return 1;

    // 3. İkon Verilerini Doldur
    strncpy(icons[icon_count].vfs_name, path, 63); 
    strncpy(icons[icon_count].label, filename, 31); 

    // 4. Tip Belirleme (vfs_stat kullanarak)
    vfs_stat_t st;
    if (vfs_stat(path, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            icons[icon_count].app_id = 1; // Klasör ikonu ID'si
        } else if (ends_with(filename, ".txt")) {
            icons[icon_count].app_id = 4; // 4: Notepad (Metin Dosyası)
        } else {
            icons[icon_count].app_id = 0; // 0: Genel Dosya
        }
    } else {
        // Stat başarısız olsa bile (RamFS bazen dizin statında hata verebilir) 
        // manuel bir kontrol: Dosya uzantısı yoksa klasör muamelesi yapabilirsin
        icons[icon_count].app_id = (strchr(filename, '.') == NULL) ? 1 : 0;
    }

    // 5. Grid Konumu
    icons[icon_count].x = 40 + (icon_count / 5 * 100);
    icons[icon_count].y = 40 + (icon_count % 5 * 90);
    icons[icon_count].is_selected = false;
    icons[icon_count].dragging = false;

    icon_count++;
    return 1;
}

void desktop_icons_init(void) {
    icon_count = 0;
    
    // 1. Önce VFS'yi tara
    vfs_list("/home/desktop", desktop_load_callback, 0);

    // 2. Eğer VFS boşsa, test için statik ikon ekle
    if (icon_count == 0) {
        strncpy(icons[0].label, "Notepad", 31);
        icons[0].x = 40;
        icons[0].y = 40;
        icons[0].app_id = 4;
        icons[0].is_selected = false;
        icon_count = 1;
    }
}

void desktop_icons_draw_all(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int i = 0; i < icon_count; i++) {
        desktop_icon_t* icon = &icons[i];
        
        // 1. Hover/Seçim Arka Planı
        bool is_hover = (mx >= icon->x && mx <= icon->x + 32 &&
                         my >= icon->y && my <= icon->y + 32);

        if (icon->is_selected) {
            gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 50, 0x0055AA);
        } else if (is_hover) {
            gfx_fill_rect(icon->x - 4, icon->y - 4, 40, 50, 0x333333);
        }

        // 2. İkon Bitmap Çizimi (DİKKAT: r ve c koordinatları)
        for (int r = 0; r < 20; r++) {
            for (int c = 0; c < 20; c++) {
                uint8_t p = 0;
                
                // İkon Seçimi
                if (icon->app_id == 1) { // 1 = Klasör (vfs_stat'tan gelen tip)
                    p = folder_icon[r][c];
                } else if (icon->app_id == 4) { // 4 = Notepad
                    p = text_file_icon[r][c];
                } else {
                    p = generic_file_icon[r][c];
                }

                uint32_t color = 0;
                // Renk Belirleme (Klasör için yeni renkler ekledik)
                if (p == 1)      color = 0x000000; // Kenarlık
                else if (p == 2) color = 0xFFCC00; // Klasör Sarısı
                else if (p == 3) color = 0xCC9900; // Klasör Gölgesi
                else if (p == 4) color = 0xFFFFFF; // Dosya Beyazı
                
                // Eğer dosya ikonu (p=2 beyaz demekti) çiziyorsan karışıklık olmasın diye:
                if (icon->app_id != 1 && p == 2) color = 0xFFFFFF;

                if (p != 0) {
                    fb_putpixel(icon->x + c + 6, icon->y + r + 6, color);
                }
            }
        }

        // 3. İkon Metni (İkonun altına merkezle)
        uint32_t text_color = (icon->is_selected || is_hover) ? 0xFFFFFF : 0xEEEEEE;
        gfx_draw_text(icon->x - 4, icon->y + 30, text_color, icon->label);
    }
}

int desktop_icons_get_hit(int mx, int my) {
    for (int i = 0; i < icon_count; i++) {
        if (mx >= icons[i].x && mx <= icons[i].x + 32 &&
            my >= icons[i].y && my <= icons[i].y + 32) return i;
    }
    return -1;
}

desktop_icon_t* desktop_icons_get_at(int index) {
    if (index < 0 || index >= icon_count) return NULL;
    return &icons[index];
}

void desktop_icons_update_selection(int x1, int y1, int x2, int y2) {
    (void)x1; (void)y1; (void)x2; (void)y2;
}

void desktop_icons_set_snap(bool enable) {
    snap_to_grid = enable;
    if (enable) desktop_icons_snap_all(); // Açıldığı an her şeyi hizala
}

void desktop_icons_deselect_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].is_selected = false;
    }
}

// Belirli bir ikonu indeksi üzerinden seçili yapar
void desktop_icons_select(int index) {
    if (index >= 0 && index < icon_count) {
        icons[index].is_selected = true;
    }
}

void desktop_icons_process_click(int index) {
    if (index < 0) return;
    desktop_icon_t* icon = &icons[index];
    if (icon->app_id > 0) {
        appmgr_start_app(icon->app_id);
    }
}

// Sürüklenen ikonun konumunu fareye göre güncelle
void desktop_icons_move_dragging(int mx, int my) {
    for (int i = 0; i < icon_count; i++) {
        if (icons[i].dragging) {
            // Fareyi ikonun ortasına (32x32 ikon için 16px ofset) hizala
            icons[i].x = mx - 16;
            icons[i].y = my - 16;
        }
    }
}

// Belirli bir ikonun sürükleme durumunu değiştir
void desktop_icons_set_dragging(int index, bool state) {
    if (index >= 0 && index < icon_count) {
        icons[index].dragging = state;
        icons[index].is_selected = state; // Sürüklenirken seçili görünsün
    }
}

// Bırakıldığında tüm sürüklemeleri durdur
void desktop_icons_stop_dragging_all(void) {
    for (int i = 0; i < icon_count; i++) {
        icons[i].dragging = false;
    }
}

void desktop_icons_snap_all(void) {
    if (!snap_to_grid) return;

    for (int i = 0; i < icon_count; i++) {
        // 80x80 piksellik hücreler kullanalım
        // 10 piksel de kenarlardan boşluk (offset) bırakalım
        int grid_x = 80;
        int grid_y = 80;
        int offset = 10;

        icons[i].x = (icons[i].x / grid_x) * grid_x + offset;
        icons[i].y = (icons[i].y / grid_y) * grid_y + offset;
        
        // Topbar ile çakışmaması için Y koordinatını kontrol et
        if (icons[i].y < 35) icons[i].y = 40; 
    }
}

// desktop_icons.c içine ekle
void desktop_icons_select_in_rect(int x1, int y1, int x2, int y2) {
    // Koordinatları normalize et (x1 her zaman küçük, x2 her zaman büyük olsun)
    int min_x = (x1 < x2) ? x1 : x2;
    int max_x = (x1 > x2) ? x1 : x2;
    int min_y = (y1 < y2) ? y1 : y2;
    int max_y = (y1 > y2) ? y1 : y2;

    for (int i = 0; i < icon_count; i++) {
        // İkonun merkezi veya sınırları seçim karesinin içinde mi?
        // İkonlarımızı 32x32 varsayıyoruz
        bool overlap = !(icons[i].x + 32 < min_x || 
                         icons[i].x > max_x || 
                         icons[i].y + 32 < min_y || 
                         icons[i].y > max_y);

        if (overlap) {
            icons[i].is_selected = true;
        }
    }
}

// desktop_icons.c içine ekle
void desktop_icons_delete_selected(void) {
    // i-- kullanarak sondan başla, liste kayınca indeksler bozulmasın
    for (int i = icon_count - 1; i >= 0; i--) {
        if (icons[i].is_selected) {
            vfs_remove(icons[i].vfs_name);
            // Elemanı diziden çıkar
            for (int j = i; j < icon_count - 1; j++) {
                icons[j] = icons[j + 1];
            }
            icon_count--;
        }
    }
}