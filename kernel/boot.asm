[bits 16]
[org 0x7c00]

KERNEL_OFFSET equ 0x1000

start:
    ; RM init
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    ; text mode
    mov ax, 0x0003
    int 0x10

    ; BIOS drive
    mov [BOOT_DRIVE], dl

    ; load kernel
    mov bx, KERNEL_OFFSET
    mov dh, 32
    mov dl, [BOOT_DRIVE]
    call disk_load

    ; rm -> pm
    call switch_to_pm
    jmp $

%include "gdt.asm"

disk_load:
    pusha
    xor ax, ax
    mov es, ax            ; ES:BX = dst

    mov ah, 0x02          ; read
    mov al, dh            ; sectors
    mov ch, 0x00          ; cyl
    mov dh, 0x00          ; head
    mov cl, 0x02          ; sector

    int 0x13
    jc disk_error
    cmp al, dh
    jne disk_error

    popa
    ret

disk_error:
    mov ah, 0x0e
    mov al, 'D'
    int 0x10
    jmp $


switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:init_pm

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov esp, 0x90000      ; stack
    call KERNEL_OFFSET   ; kernel owns us now
    jmp $

BOOT_DRIVE db 0

times 510-($-$$) db 0
dw 0xaa55
