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
ebr_volume_label:           db 'MY     OS'        ; 11 bytes, padded with spaces
ebr_system_id:              db 'FAT12   '           ; 8 bytes


; --- USEFUL REGISTERS ---
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
    ; data segments - ds, es
    mov ax, 0
    mov ds, ax
    mov es, ax

    ; stack init
    mov ss, ax
    mov sp, 0x7C00 ; is put at the begining of an os as it grows downwords
    
    ; making sure address 0x7c00 is an offset not the address
    push es
    push word .after
    retf

.after:
    ; read test
    mov [ebr_drive_number], dl

    mov si, msg_loading
    call print

    push es
    mov ah, 08h
    int 13h
    jc floppy_error
    pop es

    ; reading sectors and heads from disk directly using bios
    and cl, 0x3F
    xor ch, ch
    mov [bdb_sector_per_track], cx

    inc dh
    mov [bdb_heads], dh

    ; read FAT root directory
    
    ; compute LBA of root directory
    mov ax, [bdb_sectors_per_fat]
    mov bl, [bdb_fat_count]
    xor bh, bh
    mul bx
    add ax, [bdb_reserved_sectors]
    push ax

    ; compute size of root directory (32 * number of entries) / bytes per sector
    mov ax, [bdb_dir_entries_count]
    shl ax, 5
    xor dx, dx
    div word [bdb_bytes_per_sector]

    test dx, dx                         ; if dx != 0, add 1
    jz .root_dir_after
    inc ax

.root_dir_after:

    ; read root directory
    mov cl, al                          ; cl = number of sectors to read = size of root directory
    pop ax                              ; ax = LBA of root directory
    mov dl, [ebr_drive_number]          ; dl = drive number
    mov bx, buffer                      ; es:bx = buffer
    call disk_read

    ; search for stage2.bin
    xor bx, bx
    mov di, buffer

.search_stage2:
    mov si, file_stage2_bin
    mov cx, 11 ; as 11 is a length of a filename in FAT12
    push di
    repe cmpsb ; compares si and di bytes for repe(cx) times
    pop di
    je .found_stage2

    add di, 32
    inc bx
    cmp bx, [bdb_dir_entries_count]
    jl .search_stage2

    jmp stage2_not_found_error

.found_stage2:
    mov ax, [di + 26] ; 26 is offset of the first cluster
    mov [stage2_cluster], ax

    ; load FAR from disk into memory
    mov ax, [bdb_reserved_sectors]
    mov bx, buffer
    mov cl, [bdb_sectors_per_fat]
    mov dl, [ebr_drive_number]
    call disk_read

    ; read stage2
    mov bx, stage2_load_segment
    mov es, bx
    mov bx, stage2_load_offset

.load_stage2_loop:

    mov ax, [stage2_cluster]
    add ax, 31
    mov cl, 1
    mov dl, [ebr_drive_number]
    call disk_read

    add bx, [bdb_reserved_sectors]

    ; compute next cluster
    mov ax, [stage2_cluster]
    mov cx, 3
    mul cx
    mov cx, 2
    div cx ; ax now an entry, dx now a mod 2

    mov si, buffer
    add si, ax
    mov ax, [ds:si]

    or dx, dx
    jz .even

.odd:
    shr ax, 4
    jmp .next_cluster_after

.even:
    add ax, 0x0FFF

.next_cluster_after:
    cmp ax, 0x0FF8
    jae .read_finished

    mov [stage2_cluster], ax
    jmp .load_stage2_loop

.read_finished:
    mov dl, [ebr_drive_number]
    mov ax, stage2_load_segment
    mov ds, ax
    mov es, ax
    
    jmp stage2_load_segment:stage2_load_offset

    jmp wait_key_and_reboot

    cli
    hlt

floppy_error:
    mov si, msg_read_failed
    call print
    jmp wait_key_and_reboot

stage2_not_found_error:
    mov si, msg_stage2_not_found
    call print
    call wait_key_and_reboot

wait_key_and_reboot:
    mov ah, 0
    int 16h ; wait for key press
    jmp 0FFFFh:0 ; jump to begining of BIOS

.halt:
    cli ; disable interrupts
    hlt


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

msg_loading: db 'Loading...', ENDL, 0
msg_read_failed: db 'Read failed', ENDL, 0
file_stage2_bin: db 'STAGE2  BIN'
msg_stage2_not_found: db 'Stage 2 bootloader not found'
stage2_cluster: dw 0

stage2_load_segment: equ 0x2000
stage2_load_offset: equ 0

times 510 - ($ - $$) db 0 ; signature write
dw 0AA55h

buffer: