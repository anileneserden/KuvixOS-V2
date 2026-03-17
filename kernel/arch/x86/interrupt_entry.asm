[BITS 32]
extern timer_handler
extern mouse_handler
extern kbd_handler
extern handle_syscall      ; C tarafındaki syscall fonksiyonu

global timer_handler_asm
global mouse_handler_asm
global keyboard_handler_asm
global dummy_handler_asm
global syscall_handler_asm ; Linker için dışarı açtık

; --- GÜVENLİ KESME GİRİŞİ (TIMER) ---
timer_handler_asm:
    pushad          
    mov ax, 0x10    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call timer_handler
    popad           
    iretd

; --- GÜVENLİ KESME GİRİŞİ (KEYBOARD) ---
keyboard_handler_asm:
    pushad          
    mov ax, 0x10    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call kbd_handler
    popad           
    iretd

; --- GÜVENLİ KESME GİRİŞİ (MOUSE) ---
mouse_handler_asm:
    pushad          
    mov ax, 0x10    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call mouse_handler
    popad           
    iretd

; --- YENİ: GÜVENLİ SYSCALL GİRİŞİ (int 0x80) ---
syscall_handler_asm:
    pushad              ; Tüm registerları (EAX, EBX, ECX...) stack'e it
    
    mov ax, 0x10        ; Kernel data segmentini yükle
    mov ds, ax
    mov es, ax
    
    ; Register değerlerini parametre olarak geçiyoruz
    ; C tarafı: void handle_syscall(uint32_t eax, uint32_t ebx, uint32_t ecx)
    push ecx            ; 3. Argüman (Duration)
    push ebx            ; 2. Argüman (String Pointer)
    push eax            ; 1. Argüman (Syscall ID)
    
    call handle_syscall
    
    add esp, 12         ; Pushladığımız 3 parametreyi stackten temizle
    popad               ; Registerları geri yükle
    iretd

; --- GÜVENLİ DUMMY GİRİŞİ ---
dummy_handler_asm:
    push eax
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    pop eax
    iretd