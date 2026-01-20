AS := nasm
CC := gcc
LD := ld

ASFLAGS_BIN := -f bin
ASFLAGS_ELF := -f elf32

CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pie -fno-pic -O1 -Wall -Wextra

LDFLAGS := -m elf_i386 -T linker.ld

BUILD := build

all: $(BUILD)/uwuos.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.bin: boot.asm | $(BUILD)
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(BUILD)/entry.o: entry.asm | $(BUILD)
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD)/isr_stub.o: isr.asm | $(BUILD)
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(BUILD)/kernel.o: kernel.c types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/idt.o: idt.c idt.h types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/isr.o: isr.c types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/irq.o: irq.c types.h idt.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/network.o: network.c network.h types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/rtl8139.o: rtl8139.c network.h types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/fs.o: fs.c fs.h types.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/idt.o $(BUILD)/isr.o $(BUILD)/isr_stub.o $(BUILD)/irq.o $(BUILD)/network.o $(BUILD)/rtl8139.o $(BUILD)/fs.o linker.ld
	$(LD) $(LDFLAGS) $(BUILD)/entry.o $(BUILD)/kernel.o $(BUILD)/idt.o $(BUILD)/isr.o $(BUILD)/isr_stub.o $(BUILD)/irq.o $(BUILD)/network.o $(BUILD)/rtl8139.o $(BUILD)/fs.o -o $@

$(BUILD)/uwuos.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin
	objcopy -O binary $(BUILD)/kernel.bin $(BUILD)/kernel_flat.bin
	cat $(BUILD)/boot.bin $(BUILD)/kernel_flat.bin > $@
	truncate -s 1440K $@

run: $(BUILD)/uwuos.img
	qemu-system-i386 -drive format=raw,file=$(BUILD)/uwuos.img -m 128M -netdev user,id=net0 -device rtl8139,netdev=net0

clean:
	rm -rf $(BUILD)

.PHONY: all clean run
