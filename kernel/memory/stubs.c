// // kernel/memory/stubs.c
#include <stdint.h>
#include <stddef.h>
#include <kernel/printk.h>
#include <kernel/memory/stubs.h>

// Harici kernel bellek fonksiyonları
extern void* kmalloc(size_t size);
extern void kfree(void* ptr);

// 1. Bellek Yönetimi Köprüleri (malloc / free / realloc / calloc)
void free(void* ptr) {
    if (ptr) {
        kfree(ptr);
    }
}

void* malloc(size_t size) {
    return kmalloc(size);
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return kmalloc(size);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    void* new_ptr = kmalloc(size);
    if (new_ptr) {
        __builtin_memcpy(new_ptr, ptr, size);
        kfree(ptr);
    }
    return new_ptr;
}

void* calloc(size_t num, size_t size) {
    size_t total = num * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        __builtin_memset(ptr, 0, total);
    }
    return ptr;
}

// 2. Assert Hata Denetimi Köprüsü
void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    printk("[ASSERT FAIL] %s:%u: %s: Assertion '%s' failed.\n", file, line, function, assertion);
    while (1) { asm volatile("cli; hlt"); }
}

// ==========================================
// 3. Soft-Float, Tür Dönüşümleri ve Matematik Stub'ları
// ==========================================

// Tek duyarlıklı (float) temel matematiksel işlemler
float __mulsf3(float a, float b) { return a * b; }
float __divsf3(float a, float b) { return a / b; }
float __addsf3(float a, float b) { return a + b; }
float __subsf3(float a, float b) { return a - b; }

// Çift duyarlıklı (double) temel matematiksel işlemler
double __muldf3(double a, double b) { return a * b; }
double __divdf3(double a, double b) { return a / b; }
double __adddf3(double a, double b) { return a + b; }
double __subdf3(double a, double b) { return a - b; }

// Tür dönüşümleri (Integer <-> Float)
float __floatsisf(int32_t a) { return (float)a; }
float __floatunsisf(unsigned int a) { return (float)a; }
int32_t __fixsfsi(float a) { return (int32_t)a; }
double __floatsidf(int32_t a) { return (double)a; }
int32_t __fixdfsi(double a) { return (int32_t)a; }

// Karşılaştırma rutinleri (Lt, Gt, Le, Ge, Eq, Ne)
int __lesf2(float a, float b) { return (a <= b) ? 0 : 1; }
int __gesf2(float a, float b) { return (a >= b) ? 0 : -1; }
int __ltsf2(float a, float b) { return (a < b) ? 0 : 1; }
int __gtsf2(float a, float b) { return (a > b) ? 0 : -1; }
int __eqsf2(float a, float b) { return (a == b) ? 0 : 1; }
int __nesf2(float a, float b) { return (a != b) ? 1 : 0; }

int __ledf2(double a, double b) { return (a <= b) ? 0 : 1; }
int __gedf2(double a, double b) { return (a >= b) ? 0 : -1; }
int __ltdf2(double a, double b) { return (a < b) ? 0 : 1; }
int __gtdf2(double a, double b) { return (a > b) ? 0 : -1; }
int __nedf2(double a, double b) { return (a != b) ? 1 : 0; }
int __eqdf2(double a, double b) { return (a == b) ? 0 : 1; }

// stb_truetype için eksik olan standart matematik fonksiyonları
double floor(double x) {
    int int_part = (int)x;
    if (x < 0 && x != int_part) {
        return int_part - 1;
    }
    return (double)int_part;
}

double ceil(double x) {
    int int_part = (int)x;
    if (x > 0 && x != int_part) {
        return int_part + 1;
    }
    return (double)int_part;
}

double fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    return x - (int)(x / y) * y;
}