bits 16

section _TEXT class=CODE

global _x86_div64_32
_x86_div64_32:

    ; make new call frame
    push bp             ; save old call frame
    mov bp, sp          ; initialize new call frame

    push bx

    ; divide upper 32 bits
    mov eax, [bp + 8]   ; eax <- upper 32 bits of dividend
    mov ecx, [bp + 12]  ; ecx <- divisor
    xor edx, edx
    div ecx             ; eax - quot, edx - remainder

    ; store upper 32 bits of quotient
    mov bx, [bp + 16]
    mov [bx + 4], eax

    ; divide lower 32 bits
    mov eax, [bp + 4]   ; eax <- lower 32 bits of dividend
                        ; edx <- old remainder
    div ecx

    ; store results
    mov [bx], eax
    mov bx, [bp + 18]
    mov [bx], edx

    pop bx

    ; restore old call frame
    mov sp, bp
    pop bp
    ret

;
; int 10h ah=0Eh
; args: character, page
;
global _x86_printCharWithInterrupt
_x86_printCharWithInterrupt:
    push bp
    mov bp, sp
    
    ;saving bx for cdecl convention sake
    push bx
    
    ; bp + 0 - old call frame
    ; bp + 2 - return address
    ; bp + 4 - first argument - character
    ; bp + 6 - second argument - page
    mov al, [bp + 4]   
    mov ah, 0x0e ; Chooses printing to screen function of bios for next 10h interrupt
    mov bh, [bp + 6]
    int 10h ; Calling of a print function of bios

    ;restoring bx for convention sake
    pop bx
    
    ;restoring rest fo the register
    mov sp, bp
    pop bp
    ret