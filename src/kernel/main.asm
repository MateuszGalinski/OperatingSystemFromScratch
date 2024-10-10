org 0x7C00 ; directive that states address - 0x7c00 is deafult os boot address 
bits 16 ; directive that it's written in 16 bits

%define ENDL 0x0D, 0x0A

; --- USEFUL REGISTER ---
; CS - currently running code segment (Program counter tells about offset)
; DS - data segment
; SS - stack segment
; ES, FS, GS - extra (data) segments

; SP - stack pointer
; (((AL,AH)AX)EAX)RAX - return value from function register (or just scratch register):
;           - AL - 0 - 7 bits
;           - AH - 8 - 15 bits
;           - AX - 0 - 15 bits
;           - EAX - 0 - 31 bits
;           - RAX - 0 - 63 bits (ONLY 64 bit mode)
; 

; --- INTERUPTS ---
; int 10h prints to a screen in tty mode if
; AH = 0E
; AL - asci ro print
; BH - page number

; -----------------------------------CODE----------------------------------

start:
    jmp main

    
; Prints string to a screen
; Params:
; - ds:si pointer to the string
print:
    push si
    push ax

.loop:
    lodsb ; loads al
    or al, al
    jz .done ; jumps if or al, al is invoked for null character

    mov ah, 0x0e ; Chooses printing to screen function of bios for next 10h interrupt(not to be confused with mine)
    mov bh, 0 ; no need to put ascii to AL as we do it in earlier lines
    int 10h ; Calling of a print function of bios

    jmp .loop

.done:
    pop ax
    pop si
    ret

main:
    ; data segments - ds, es
    mov ax, 0
    mov ds, ax
    mov es, ax

    ; stack init
    mov ss, ax
    mov sp, 0x7C00 ; is put at the begining of an os as it grows downwords
    
    mov si, msg_hello
    call print

    hlt

.halt:
    jmp .halt

msg_hello: db 'Hello world!', ENDL, 0

times 510 - ($ - $$) db 0 ; signature write
dw 0AA55h
