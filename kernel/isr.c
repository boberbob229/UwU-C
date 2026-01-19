#include "types.h"

struct regs {
    u32 gs, fs, es, ds;
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 int_no, err_code;
    u32 eip, cs, eflags, useresp, ss;
};

void isr_handler(struct regs* r) {
    (void)r;

    volatile u16* vga = (u16*)0xB8000;
    vga[0] = 0x4F45;
    vga[1] = 0x4F52;
    vga[2] = 0x4F52;

    for (;;) {
        asm volatile("cli; hlt");
    }
}
