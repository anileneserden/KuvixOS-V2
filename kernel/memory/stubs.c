// kernel/memory/stubs.c
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
// 3. IEEE 754 Soft-Float ve Tür Dönüşümleri
// ==========================================

static inline uint32_t f2u(float f) {
    uint32_t u;
    __builtin_memcpy(&u, &f, sizeof(u));
    return u;
}

static inline float u2f(uint32_t u) {
    float f;
    __builtin_memcpy(&f, &u, sizeof(f));
    return f;
}

static inline uint64_t d2u(double d) {
    uint64_t u;
    __builtin_memcpy(&u, &d, sizeof(u));
    return u;
}

static inline double u2d(uint64_t u) {
    double d;
    __builtin_memcpy(&d, &u, sizeof(d));
    return d;
}

int __lesf2(float a, float b) {
    uint32_t ua = f2u(a), ub = f2u(b);
    if ((ua & 0x7fffffffU) == 0 && (ub & 0x7fffffffU) == 0) return 0;
    int sa = (ua >> 31) != 0;
    int sb = (ub >> 31) != 0;
    if (sa != sb) return sa ? -1 : 1;
    if (ua == ub) return 0;
    if (!sa) return (ua < ub) ? -1 : 1;
    return (ua > ub) ? -1 : 1;
}

int __ltsf2(float a, float b) { return (__lesf2(a, b) < 0) ? -1 : 0; }
int __gesf2(float a, float b) { return (__lesf2(a, b) >= 0) ? 0 : -1; }
int __gtsf2(float a, float b) { return (__lesf2(a, b) > 0) ? 1 : 0; }
int __eqsf2(float a, float b) {
    uint32_t ua = f2u(a), ub = f2u(b);
    if ((ua & 0x7fffffffU) == 0 && (ub & 0x7fffffffU) == 0) return 0;
    return (ua == ub) ? 0 : 1;
}
int __nesf2(float a, float b) { return (__eqsf2(a, b) != 0) ? 1 : 0; }

float __floatsisf(int32_t a) {
    if (a == 0) return 0.0f;
    uint32_t sign = 0;
    uint32_t mag = (uint32_t)a;
    if (a < 0) {
        sign = 0x80000000U;
        mag = (uint32_t)(0u - (uint32_t)a);
    }
    int msb = 31;
    while (msb > 0 && ((mag >> msb) & 1u) == 0) msb--;
    int exp = msb + 127;
    uint32_t mant;
    if (msb > 23) {
        mant = (mag >> (msb - 23)) & 0x7fffffU;
    } else {
        mant = (mag << (23 - msb)) & 0x7fffffU;
    }
    return u2f(sign | ((uint32_t)exp << 23) | mant);
}

float __floatunsisf(uint32_t a) {
    if (a == 0) return 0.0f;
    int msb = 31;
    while (msb > 0 && ((a >> msb) & 1u) == 0) msb--;
    int exp = msb + 127;
    uint32_t mant = (msb > 23) ? ((a >> (msb - 23)) & 0x7fffffU)
                               : ((a << (23 - msb)) & 0x7fffffU);
    return u2f(((uint32_t)exp << 23) | mant);
}

int32_t __fixsfsi(float a) {
    uint32_t u = f2u(a);
    int sign = (u >> 31) != 0;
    int exp = (int)((u >> 23) & 0xffU) - 127;
    if (exp < 0) return 0;
    if (exp >= 31) return sign ? (-2147483647 - 1) : 2147483647;
    uint32_t mant = (u & 0x7fffffU) | 0x800000U;
    uint32_t val;
    if (exp > 23) {
        val = mant << (exp - 23);
    } else {
        val = mant >> (23 - exp);
    }
    return sign ? -(int32_t)val : (int32_t)val;
}

float __addsf3(float a, float b) {
    uint32_t ua = f2u(a), ub = f2u(b);
    if ((ua & 0x7fffffffU) == 0) return b;
    if ((ub & 0x7fffffffU) == 0) return a;

    int sa = (ua >> 31) ? -1 : 1;
    int sb = (ub >> 31) ? -1 : 1;
    int ea = (int)((ua >> 23) & 0xffU) - 127;
    int eb = (int)((ub >> 23) & 0xffU) - 127;
    int64_t ma = (int64_t)((ua & 0x7fffffU) | 0x800000U) * sa;
    int64_t mb = (int64_t)((ub & 0x7fffffU) | 0x800000U) * sb;

    int e = ea;
    if (ea > eb) {
        int shift = ea - eb;
        mb = (shift >= 60) ? 0 : (mb >> shift);
        e = ea;
    } else if (eb > ea) {
        int shift = eb - ea;
        ma = (shift >= 60) ? 0 : (ma >> shift);
        e = eb;
    }

    int64_t sum = ma + mb;
    if (sum == 0) return 0.0f;

    uint32_t sign = 0;
    uint64_t abs_sum = (uint64_t)sum;
    if (sum < 0) {
        sign = 0x80000000U;
        abs_sum = (uint64_t)(0 - sum);
    }

    int msb = 63;
    while (msb > 0 && ((abs_sum >> msb) & 1ULL) == 0) msb--;
    e += (msb - 23);
    if (e <= -126) return 0.0f;
    if (e >= 127) return u2f(sign | (0x7fU << 23) | 0x7fffffU);

    uint32_t mant;
    if (msb > 23) {
        mant = (uint32_t)(abs_sum >> (msb - 23)) & 0x7fffffU;
    } else {
        mant = (uint32_t)(abs_sum << (23 - msb)) & 0x7fffffU;
    }
    return u2f(sign | ((uint32_t)(e + 127) << 23) | mant);
}

float __subsf3(float a, float b) {
    uint32_t ub = f2u(b) ^ 0x80000000U;
    return __addsf3(a, u2f(ub));
}

float __mulsf3(float a, float b) {
    uint32_t ua = f2u(a), ub = f2u(b);
    if ((ua & 0x7fffffffU) == 0 || (ub & 0x7fffffffU) == 0) return 0.0f;

    uint32_t sign = (ua ^ ub) & 0x80000000U;
    int ea = (int)((ua >> 23) & 0xffU) - 127;
    int eb = (int)((ub >> 23) & 0xffU) - 127;
    uint64_t ma = (ua & 0x7fffffU) | 0x800000U;
    uint64_t mb = (ub & 0x7fffffU) | 0x800000U;

    uint64_t prod = ma * mb;
    int e = ea + eb;
    int msb = 63;
    while (msb > 0 && ((prod >> msb) & 1ULL) == 0) msb--;
    e += (msb - 46);

    if (e <= -126) return 0.0f;
    if (e >= 127) return u2f(sign | (0x7fU << 23) | 0x7fffffU);

    uint32_t mant;
    if (msb > 23) {
        mant = (uint32_t)(prod >> (msb - 23)) & 0x7fffffU;
    } else {
        mant = (uint32_t)(prod << (23 - msb)) & 0x7fffffU;
    }
    return u2f(sign | ((uint32_t)(e + 127) << 23) | mant);
}

float __divsf3(float a, float b) {
    uint32_t ua = f2u(a), ub = f2u(b);
    if ((ua & 0x7fffffffU) == 0) return 0.0f;
    if ((ub & 0x7fffffffU) == 0) return 0.0f;

    uint32_t sign = (ua ^ ub) & 0x80000000U;
    int ea = (int)((ua >> 23) & 0xffU) - 127;
    int eb = (int)((ub >> 23) & 0xffU) - 127;
    uint64_t ma = ((uint64_t)((ua & 0x7fffffU) | 0x800000U)) << 25;
    uint64_t mb = (uint64_t)((ub & 0x7fffffU) | 0x800000U);

    uint64_t q = ma / mb;
    int e = ea - eb;

    int msb = 63;
    while (msb > 0 && ((q >> msb) & 1ULL) == 0) msb--;
    e += (msb - 25);

    if (e <= -126) return 0.0f;
    if (e >= 127) return u2f(sign | (0x7fU << 23) | 0x7fffffU);

    uint32_t mant;
    if (msb > 23) {
        mant = (uint32_t)(q >> (msb - 23)) & 0x7fffffU;
    } else {
        mant = (uint32_t)(q << (23 - msb)) & 0x7fffffU;
    }
    return u2f(sign | ((uint32_t)(e + 127) << 23) | mant);
}

double __adddf3(double a, double b) {
    uint64_t ua = d2u(a), ub = d2u(b);
    if ((ua & 0x7fffffffffffffffULL) == 0) return b;
    if ((ub & 0x7fffffffffffffffULL) == 0) return a;

    // 6 bit taşma payı ile kaydırılmış anlamlı kısım (53 + 6 = 59 bit -> int64_t içine sığar)
    int sa = (ua >> 63) ? -1 : 1;
    int sb = (ub >> 63) ? -1 : 1;
    int ea = (int)((ua >> 52) & 0x7ffULL) - 1023;
    int eb = (int)((ub >> 52) & 0x7ffULL) - 1023;

    int64_t ma = (int64_t)(((ua & 0x000fffffffffffffULL) | 0x0010000000000000ULL) << 6) * sa;
    int64_t mb = (int64_t)(((ub & 0x000fffffffffffffULL) | 0x0010000000000000ULL) << 6) * sb;

    int e = ea;
    if (ea > eb) {
        int shift = ea - eb;
        mb = (shift >= 62) ? 0 : (mb >> shift);
        e = ea;
    } else if (eb > ea) {
        int shift = eb - ea;
        ma = (shift >= 62) ? 0 : (ma >> shift);
        e = eb;
    }

    int64_t sum = ma + mb;
    if (sum == 0) return 0.0;

    uint64_t sign = 0;
    uint64_t abs_sum;
    if (sum < 0) {
        sign = 0x8000000000000000ULL;
        abs_sum = (uint64_t)(-(sum + 1)) + 1ULL;
    } else {
        abs_sum = (uint64_t)sum;
    }

    int msb = 63;
    while (msb > 0 && ((abs_sum >> msb) & 1ULL) == 0) msb--;

    // Anlamlı bit (53. bit) + 6 bit kaydırma = 58. bit normal pozisyon
    e += (msb - 58);

    if (e <= -1022) return 0.0;
    if (e >= 1023) return u2d(sign | (0x7feULL << 52) | 0x000fffffffffffffULL);

    uint64_t mant;
    if (msb > 52) {
        mant = (abs_sum >> (msb - 52)) & 0x000fffffffffffffULL;
    } else {
        mant = (abs_sum << (52 - msb)) & 0x000fffffffffffffULL;
    }

    return u2d(sign | ((uint64_t)(e + 1023) << 52) | mant);
}

double __subdf3(double a, double b) {
    uint64_t ub = d2u(b) ^ 0x8000000000000000ULL;
    return __adddf3(a, u2d(ub));
}