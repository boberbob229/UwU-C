#ifndef IDT_H
#define IDT_H

#include "types.h"

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8  reserved;
    u8  flags;
    u16 offset_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(u8 num, u32 base, u16 sel, u8 flags);

#endif
