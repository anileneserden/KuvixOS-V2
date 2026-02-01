[BITS 32]
extern timer_handler
extern mouse_handler
extern kbd_handler       ; C tarafındaki klavye fonksiyonunu dışarıdan al

global timer_handler_asm
global mouse_handler_asm
global keyboard_handler_asm    ; Linker'ın aradığı isim artık burada!
global dummy_handler_asm

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

    call kbd_handler    ; C tarafındaki klavye fonksiyonunu çağırır

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

    call mouse_handler  ; C tarafındaki mouse_handler'ı çağırır

    popad           
    iretd

; --- GÜVENLİ DUMMY GİRİŞİ ---
dummy_handler_asm:
    push eax
    mov al, 0x20
    out 0x20, al
    out 0xA0, al
    pop eax
    iretd