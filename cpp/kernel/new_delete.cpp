#include <stddef.h> // size_t için

// kmalloc ve kfree C fonksiyonlarıdır, C++ derleyicisinin 
// bunları isimlendirme (mangling) hatasına düşmemesi için
// "C" dilinde olduklarını belirtiyoruz.
extern "C" {
    void* kmalloc(size_t size);
    void kfree(void* p);
}

// Temel new operatörleri
void* operator new(size_t size) { 
    return kmalloc(size); 
}

void* operator new[](size_t size) { 
    return kmalloc(size); 
}

// Temel delete operatörleri
void operator delete(void* p) { 
    kfree(p); 
}

void operator delete[](void* p) { 
    kfree(p); 
}

// Sized Deallocation (C++14/17 standardı için zorunlu)
void operator delete(void* p, size_t size) {
    (void)size; // Boyut bilgisini burada kullanmıyoruz, kfree yeterli
    kfree(p);
}

void operator delete[](void* p, size_t size) {
    (void)size;
    kfree(p);
}