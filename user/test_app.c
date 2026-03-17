// user/test_app.c
void _start() {
    const char* msg = "Merhaba KuvixOS v3!";
    
    // EAX: 100 (Syscall ID), EBX: Mesaj, ECX: Süre (ms)
    asm volatile (
        "mov $100, %%eax\n"
        "mov %0, %%ebx\n"
        "mov $1500, %%ecx\n"
        "int $0x80\n"
        : : "r"(msg) : "eax", "ebx", "ecx"
    );

    while(1); // Kernel'a dönmemesi için
}