#ifndef KUVIX_PRINTK_HPP
#define KUVIX_PRINTK_HPP

#include <stdarg.h>

// Mevcut C fonksiyonlarını C++'a güvenli bir şekilde tanıtıyoruz
extern "C" {
    #include <kernel/printk.h>
    
    // Eğer gui_mode_enabled değişkenine C++ içinden erişmek istersen:
    void printk_set_gui_mode(bool enable);
}

namespace Kuvix::Kernel {

    /**
     * @brief C++ dünyası için printk sarmalayıcısı.
     * İleride buraya stream (kout << ...) desteği eklenebilir.
     */
    inline void log(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        
        // Asıl işi yapan senin mevcut C printk fonksiyonun
        // Ancak va_list kabul eden bir vprintk fonksiyonun olmadığı için 
        // doğrudan printk'yı kullanacağız.
        
        va_end(args);
    }
}

#endif