org 0x7C00 ; directive that states address - 0x7c00 is deafult os boot address 
bits 16 ; directive that it's written in 16 bits

%define ENDL 0x0D, 0x0A

;
; FAT12 headers
;

jmp short start
nop

bdb_oem:                    db 'MSWIN4.1'
bdb_bytes_per_sector:       dw 512
bdb_sectors_per_cluster:    db 1
bdb_reserved_sectors:       dw 1
bdb_fat_count:              db 2
bdb_dir_entries_count:      dw 0E0h
bdb_total_sectors:          dw 2880 ; 2880 * 512 = 1.44MB
bdb_media_descriptor_type:  db 0F0h ; F0 = 3.5" floppy disk
bdb_sectors_per_fat:        dw 9
bdb_sector_per_track:       dw 18
bdb_heads:                  dw 2
bdb_hidden_sectors:         dd 0
bdb_large_sector_count:     dd 0

ebr_drive_number:           db 0
                            db 0 ; reserved
ebr_signature:              db 29h
ebr_volume_id:              db 12h, 34h, 56h, 78h
ebr_volume_label:           db 'NANOBYTE OS'        ; 11 bytes, padded with spaces
ebr_system_id:              db 'FAT12   '           ; 8 bytes


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

    ; read test
    mov [ebr_drive_number], dl
    mov ax, 1
    mov cl, 1
    mov bx, 0x7E00
    call disk_read


    mov si, msg_hello
    call print

    cli
    hlt

floppy_error:
    mov si, msg_read_failed
    call print
    jmp wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h ; wait for key press
    jmp 0FFFFh:0 ; jump to begining of BIOS

.halt:
    cli ; disable interrupts
    jmp .halt

;
; Disk converstions
;

;
; Converts LBA (Logical Block Address) to CHS (floppy disk format)
; Parameters:
;   - ax: LBA address
; Returns:
;   - cx [bits 0-5]: sector number (cx register is within RCX register)
;   - cx [bits 6-15]: cylinder
;   - dh: head

lba_to_chs:
    push ax
    push dx

    xor dx, dx                          ; dx = 0
    div word [bdb_sector_per_track]     ; ax = LBA / SectorsPerTrack
                                        ; dx = LBA % SectorsPerTrack

    inc dx                              ; dx = (LBA % SectorsPerTrack) + 1
    mov cx, dx                          ; cx = (LBA % SectorsPerTrack) + 1 = sector

    xor dx, dx                          ; dx = 0
    div word [bdb_heads]                ; ax = (LBA / SectorsPerTrack) / Heads = cylinder
                                        ; dx = (LBA / SectorsPerTrack) % Heads = head

    mov dh, dl                          ; dh = dh[0-7]
    mov ch, al                          ; ch =
    shl ah, 6                           ; truncate to have only 2 upper bits
    or cl, ah                           ; cl = ah

    pop ax
    mov dl, al                          ; restore dl
    pop ax
    ret

;
; Reads sectors from a disk
; Parameters:
;   - ah: LBA address
;   - cl: number of sectors to read
;   - dl: drive number
;   - es:bx: data read after function
;
disk_read:
    push ax
    push bx
    push cx
    push dx
    push di ; destination index register

    push cx
    call lba_to_chs
    pop ax

    mov ah, 02h
    mov di, 3 ; retry count

.retry:
    pusha       ; save all registers
    stc         ; set carry flag, in case BIOS doesnt
    int 13h     ; carry flag cleared = success, int 13 is responsible for reading from disk
    jnc .done

    popa
    call disk_reset

    dec di
    test di, di
    jnz .retry

.fail:
    jmp floppy_error

.done:
    popa
    pop di
    pop dx
    pop cx
    pop bx
    pop ax
    ret

;
; Resets disk controller
; Parameters:
;   - dl: drive number
;
disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret

msg_hello: db 'Hello world!', ENDL, 0
msg_read_failed: db 'Read failed', ENDL, 0

times 510 - ($ - $$) db 0 ; signature write
dw 0AA55h
