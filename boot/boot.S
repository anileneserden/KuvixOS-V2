# Multiboot bayrakları
.set ALIGN,             1 << 0
.set MEMINFO,           1 << 1
.set VIDEO_MODE,        1 << 2  # <--- Grafik modunu isteyen bayrak

# Bayrakları birleştir (0x00000007 olur)
.set MULTIBOOT_MAGIC,    0x1BADB002
.set MULTIBOOT_FLAGS,    ALIGN | MEMINFO | VIDEO_MODE
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section .multiboot
.align 4
.long MULTIBOOT_MAGIC
.long MULTIBOOT_FLAGS
.long MULTIBOOT_CHECKSUM

# Grafik modu gereksinimleri (Multiboot Header devamı)
# Bu alanlar sadece VIDEO_MODE (bit 2) aktifse okunur
.long 0, 0, 0, 0, 0     # Mod bilgisi için ayrılmış alanlar
.long 0                 # 0 = Lineer Framebuffer (LFB) modu
.long 800               # Genişlik
.long 600               # Yükseklik
.long 32                # BPP (Bit Per Pixel)

.section .text
.global _start
.extern kernel_main
.extern stack_top

_start:
    cli
    mov $stack_top, %esp
    
    # Parametreler sağdan sola itilir (cdecl)
    push %ebx        # 2. parametre: multiboot_info_t* (mbi)
    push %eax        # 1. parametre: uint32_t (magic)
    
    call kernel_main

.hang:
    hlt
    jmp .hang