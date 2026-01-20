#include "types.h"
#include "network.h"

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID_82540EM 0x100E

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define E1000_REG_CTRL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_EECD 0x0010
#define E1000_REG_EERD 0x0014
#define E1000_REG_ICR 0x00C0
#define E1000_REG_IMS 0x00D0
#define E1000_REG_RCTL 0x0100
#define E1000_REG_TCTL 0x0400
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH 0x2810
#define E1000_REG_RDT 0x2818
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH 0x3810
#define E1000_REG_TDT 0x3818
#define E1000_REG_RAL 0x5400
#define E1000_REG_RAH 0x5404

#define RX_DESC_COUNT 16
#define TX_DESC_COUNT 8

extern void net_handle_packet(u8* packet, u32 len);

typedef struct {
    u64 addr;
    u16 len;
    u16 checksum;
    u8  status;
    u8  errors;
    u16 special;
} __attribute__((packed)) e1000_rx_desc_t;

typedef struct {
    u64 addr;
    u16 len;
    u8  cso;
    u8  cmd;
    u8  status;
    u8  css;
    u16 special;
} __attribute__((packed)) e1000_tx_desc_t;

static e1000_rx_desc_t rx_descs[RX_DESC_COUNT] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[TX_DESC_COUNT] __attribute__((aligned(16)));
static u8 rx_buffers[RX_DESC_COUNT][2048] __attribute__((aligned(16)));
static u8 tx_buffers[TX_DESC_COUNT][2048] __attribute__((aligned(16)));

static u32 mmio_base = 0;
static u8 mac_addr[6] = {0};
static u32 tx_tail = 0;

static inline void outl(u16 port, u32 val) {
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(u16 port, u16 val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static u32 mmio_read(u32 reg) {
    return *((volatile u32*)(mmio_base + reg));
}

static void mmio_write(u32 reg, u32 val) {
    *((volatile u32*)(mmio_base + reg)) = val;
}

static u32 pci_read(u8 bus, u8 slot, u8 func, u8 offset) {
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write(u8 bus, u8 slot, u8 func, u8 offset, u32 value) {
    u32 address = (u32)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static int find_e1000_device(u8* bus, u8* slot, u8* func) {
    for (u32 b = 0; b < 256; b++) {
        for (u32 s = 0; s < 32; s++) {
            for (u32 f = 0; f < 8; f++) {
                u32 vendor_device = pci_read(b, s, f, 0);
                u16 vendor = vendor_device & 0xFFFF;
                u16 device = (vendor_device >> 16) & 0xFFFF;
                
                if (vendor == E1000_VENDOR_ID && device == E1000_DEVICE_ID_82540EM) {
                    *bus = b;
                    *slot = s;
                    *func = f;
                    return 1;
                }
            }
        }
    }
    return 0;
}

void e1000_init(void) {
    u8 bus, slot, func;
    
    if (!find_e1000_device(&bus, &slot, &func)) {
        for (int i = 0; i < 6; i++) mac_addr[i] = 0;
        return;
    }
    
    u32 bar0 = pci_read(bus, slot, func, 0x10);
    mmio_base = bar0 & 0xFFFFFFF0;
    
    u32 cmd = pci_read(bus, slot, func, 0x04);
    cmd |= 0x06;
    pci_write(bus, slot, func, 0x04, cmd);
    
    u32 ral = mmio_read(E1000_REG_RAL);
    u32 rah = mmio_read(E1000_REG_RAH);
    
    mac_addr[0] = (ral >> 0) & 0xFF;
    mac_addr[1] = (ral >> 8) & 0xFF;
    mac_addr[2] = (ral >> 16) & 0xFF;
    mac_addr[3] = (ral >> 24) & 0xFF;
    mac_addr[4] = (rah >> 0) & 0xFF;
    mac_addr[5] = (rah >> 8) & 0xFF;
    
    for (u32 i = 0; i < RX_DESC_COUNT; i++) {
        rx_descs[i].addr = (u64)(u32)&rx_buffers[i][0];
        rx_descs[i].status = 0;
    }
    
    for (u32 i = 0; i < TX_DESC_COUNT; i++) {
        tx_descs[i].addr = (u64)(u32)&tx_buffers[i][0];
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1;
    }
    
    mmio_write(E1000_REG_RDBAL, (u32)rx_descs);
    mmio_write(E1000_REG_RDBAH, 0);
    mmio_write(E1000_REG_RDLEN, RX_DESC_COUNT * sizeof(e1000_rx_desc_t));
    mmio_write(E1000_REG_RDH, 0);
    mmio_write(E1000_REG_RDT, RX_DESC_COUNT - 1);
    
    mmio_write(E1000_REG_TDBAL, (u32)tx_descs);
    mmio_write(E1000_REG_TDBAH, 0);
    mmio_write(E1000_REG_TDLEN, TX_DESC_COUNT * sizeof(e1000_tx_desc_t));
    mmio_write(E1000_REG_TDH, 0);
    mmio_write(E1000_REG_TDT, 0);
    
    mmio_write(E1000_REG_RCTL, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15) | (1 << 26));
    mmio_write(E1000_REG_TCTL, (1 << 1) | (1 << 3) | (15 << 4) | (64 << 12));
    
    tx_tail = 0;
}

void e1000_send(u8* data, u32 len) {
    if (!mmio_base) return;
    if (len > 2048) len = 2048;
    
    u32 tail = tx_tail;
    
    for (u32 i = 0; i < len; i++) {
        tx_buffers[tail][i] = data[i];
    }
    
    tx_descs[tail].len = len;
    tx_descs[tail].cmd = (1 << 0) | (1 << 1) | (1 << 3);
    tx_descs[tail].status = 0;
    
    tx_tail = (tx_tail + 1) % TX_DESC_COUNT;
    mmio_write(E1000_REG_TDT, tx_tail);
    
    u32 timeout = 100000;
    while (!(tx_descs[tail].status & 0x01) && timeout--) {
        for (volatile int i = 0; i < 10; i++);
    }
}

void e1000_poll(void) {
    if (!mmio_base) return;
    
    u32 tail = mmio_read(E1000_REG_RDT);
    u32 next = (tail + 1) % RX_DESC_COUNT;
    
    while (rx_descs[next].status & 0x01) {
        u16 len = rx_descs[next].len;
        if (len > 0 && len < 2048) {
            net_handle_packet(rx_buffers[next], len);
        }
        
        rx_descs[next].status = 0;
        mmio_write(E1000_REG_RDT, next);
        
        tail = next;
        next = (next + 1) % RX_DESC_COUNT;
    }
}

void e1000_get_mac(u8* mac) {
    for (int i = 0; i < 6; i++) {
        mac[i] = mac_addr[i];
    }
}
