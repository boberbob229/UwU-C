#include "idt.h"

extern void* isr_stub_table[];

static struct idt_entry idt[256];
static struct idt_ptr idtr;

void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].reserved = 0;
    idt[num].flags = flags;
}

void idt_init(void) {
    for (u16 i = 0; i < 32; i++) {
        idt_set_gate((u8)i, (u32)isr_stub_table[i], 0x08, 0x8E);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u32)&idt;

    asm volatile("lidt %0" :: "m"(idtr));
}
