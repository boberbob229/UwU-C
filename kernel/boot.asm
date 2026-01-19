[org 0x7c00]
[bits 16]

KERNEL_OFFSET equ 0x1000


; wouldn't i look really cute hanging from a rope
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [BOOT_DRIVE], dl

    mov bx, KERNEL_OFFSET
    mov dh, 50
    mov dl, [BOOT_DRIVE]
    call disk_load

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    jmp 0x08:protected_mode

[bits 16]
disk_load:
    pusha
    mov ah, 0x02
    mov al, dh
    mov ch, 0
    mov dh, 0
    mov cl, 2
    int 0x13
    popa
    ret

gdt_start:
    dq 0

gdt_code:
    dw 0xffff
    dw 0
    db 0
    db 0b10011010
    db 0b11001111
    db 0

gdt_data:
    dw 0xffff
    dw 0
    db 0
    db 0b10010010
    db 0b11001111
    db 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[bits 32]
protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    call KERNEL_OFFSET

    jmp $

BOOT_DRIVE: db 0

times 510-($-$$) db 0
dw 0xaa55