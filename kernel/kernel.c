#include "types.h"

#define VIDEO_MEM 0xB8000
#define SCREEN_W  80
#define SCREEN_H  25

extern void idt_init(void);
extern void irq_init(void);
extern void irq_install_handler(int irq, void (*handler)(void));

static u16* vga_buffer;
static u32  cursor_y, cursor_x;
static u8   terminal_color;
static char current_path[256];
static u8 is_light_mode = 0;

#define KBD_BUFFER_SIZE 256
static u8 kbd_buffer[KBD_BUFFER_SIZE];
static volatile u32 kbd_read_pos = 0;
static volatile u32 kbd_write_pos = 0;

static u8 key_map[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static inline void outb(u16 port, u8 val) {
    asm volatile("outb %0, %1" :: "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void keyboard_handler(void) {
    u8 sc = inb(0x60);

    if (sc < 0x80 && key_map[sc]) {
        u32 next = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
        if (next != kbd_read_pos) {
            kbd_buffer[kbd_write_pos] = key_map[sc];
            kbd_write_pos = next;
        }
    }
}

u8 keyboard_getchar(void) {
    while (kbd_read_pos == kbd_write_pos) {
        asm volatile("hlt");
    }
    u8 c = kbd_buffer[kbd_read_pos];
    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
    return c;
}

void terminal_clear(void) {
    for (u32 i = 0; i < SCREEN_W * SCREEN_H; i++) {
        vga_buffer[i] = (terminal_color << 8) | ' ';
    }
    cursor_y = cursor_x = 0;
}

void terminal_scroll(void) {
    for (u32 i = 0; i < SCREEN_W * (SCREEN_H - 1); i++) {
        vga_buffer[i] = vga_buffer[i + SCREEN_W];
    }
    for (u32 i = 0; i < SCREEN_W; i++) {
        vga_buffer[SCREEN_W * (SCREEN_H - 1) + i] = (terminal_color << 8) | ' ';
    }
    cursor_y = SCREEN_H - 1;
}

void terminal_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        if (++cursor_y >= SCREEN_H) terminal_scroll();
        return;
    }
    vga_buffer[cursor_y * SCREEN_W + cursor_x] = (terminal_color << 8) | c;
    if (++cursor_x >= SCREEN_W) {
        cursor_x = 0;
        if (++cursor_y >= SCREEN_H) terminal_scroll();
    }
}

void terminal_write(const char* s) {
    while (*s) terminal_putc(*s++);
}

void terminal_writeln(const char* s) {
    terminal_write(s);
    terminal_putc('\n');
}

int str_compare(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(const u8*)a - *(const u8*)b;
}

void str_copy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void mem_set(void* dest, u8 val, u32 len) {
    u8* ptr = (u8*)dest;
    while (len--) *ptr++ = val;
}

u32 str_len(const char* s) {
    u32 len = 0;
    while (*s++) len++;
    return len;
}

extern volatile u32 ticks;

void get_cpu_brand (char * buf) {
    u32 brand[12];
    for (u32 i = 0; i < 3; i ++) {
        asm volatile("cpuid" : "=a"(brand[i * 4]), "=b"(brand[i * 4 + 1]), "=c"(brand[i * 4 + 2]), "=d"(brand[i * 4 + 3]) : "a"(0x80000002 + i));
    }
    char * p = (char *) brand;
    for (int i = 0; i < 48; i ++) {
        if (p[i] == 0) {
            break;
        }
        buf[i] = p[i];
    }
    buf[47] = 0;
}

u8 get_rtc_register (int reg) {
    outb(0x70, reg);
    return inb(0x71);
}

extern u32 _end;

u32 fetch_used_memory ( ) {
    u32 kernel_start = 0x1000;
    u32 kernel_end = (u32) & _end;
    return (kernel_end - kernel_start) / 1024;
}

u32 fetch_total_memory ( ) {
    outb(0x70, 0x17);
    u16 low = inb(0x71);
    outb(0x70, 0x18);
    u16 high = inb(0x71);
    u32 total_kb = (high << 8) | low;
    return (total_kb / 1024) + 1;
}

void int_to_str (char * buf, u32 n) {
    if (n == 0) {
        buf[0] = '0'; buf[1] = 0;
        return;
    }
    int i = 0, j;
    while (n > 0) {
        buf[i ++] = (n % 10) + '0';
        n /= 10;
    }
    buf[i] = 0;
    for (j = 0; j < i / 2; j ++) {
        char tmp = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = tmp;
    }
}

u8 bcd_to_bin (u8 val) {
    return ((val / 16) * 10) + (val % 16);
}

void get_date_string (char * buf) {
    u32 ptr = 0;
    u8 day = get_rtc_register(0x07);
    u8 month = get_rtc_register(0x08);
    u8 year = get_rtc_register(0x09);

    char d_str[16], m_str[16], y_str[16];
    int_to_str(d_str, bcd_to_bin(day));
    int_to_str(m_str, bcd_to_bin(month));
    int_to_str(y_str, bcd_to_bin(year));

    if (bcd_to_bin(day) < 10) buf[ptr ++] = '0';
    for(int i = 0; d_str[i]; i ++) buf[ptr ++] = d_str[i];
    buf[ptr ++] = '/';

    if (bcd_to_bin(month) < 10) buf[ptr ++] = '0';
    for(int i = 0; m_str[i]; i ++) buf[ptr ++] = m_str[i];
    buf[ptr ++] = '/';

    buf[ptr ++] = '2'; buf[ptr ++] = '0';
    if (bcd_to_bin(year) < 10) buf[ptr ++] = '0';
    for(int i = 0; y_str[i]; i ++) buf[ptr ++] = y_str[i];

    buf[ptr] = 0;
}

void fetch_print_info (const char * key, const char * val, u8 val_color) {
    cursor_x = 18;
    terminal_color = is_light_mode ? 0xF3 : 0x0B; terminal_write(key);
    terminal_color = is_light_mode ? 0xF0 : 0x0F; terminal_write(": ");
    terminal_color = val_color; terminal_write(val);

    cursor_y ++;
    if (cursor_y >= SCREEN_H) terminal_scroll();

};

void breadfetch (void) {
    char cpu_buf[49];
    char mem_total_buf[16];
    char mem_used_buf[16];
    char uptime_buf[16];
    char date_buf[24];

    get_cpu_brand(cpu_buf);
    get_date_string(date_buf);
    int_to_str(uptime_buf, ticks / 100);
    int_to_str(mem_total_buf, fetch_total_memory());
    int_to_str(mem_used_buf, fetch_used_memory());

    terminal_clear();
    terminal_putc('\n');
    terminal_color = is_light_mode ? 0xF8 : 0x08;
    terminal_writeln("  /\\_/\\");
    terminal_writeln(" ( o.o )");
    terminal_writeln("  > ^ <");
    terminal_writeln("  Uwu-C");
    terminal_writeln("       ");
    terminal_writeln("       ");
    terminal_writeln("       ");
    terminal_writeln("       ");

    cursor_y -= 7;

    fetch_print_info("OS", "UwU-OS", is_light_mode ? 0xFD : 0x0D);
    fetch_print_info("Date", date_buf, is_light_mode ? 0xF0 : 0x0F);
    fetch_print_info("Kernel", "crwumb-x86", is_light_mode ? 0xF0 : 0x07);

    cursor_x = 18;
    terminal_color = is_light_mode ? 0xF3 : 0x0B; terminal_write("Uptime");
    terminal_color = is_light_mode ? 0xF0 : 0x0F; terminal_write(": ");
    terminal_color = is_light_mode ? 0xF0 : 0x07; terminal_write(uptime_buf);
    terminal_write("s");
    cursor_y ++;

    fetch_print_info("CPU", cpu_buf, is_light_mode ? 0xFE : 0x0E);
    fetch_print_info("Shell", "crwumb-sh", is_light_mode ? 0xF0 : 0x07);

    cursor_x = 18;
    terminal_color = is_light_mode ? 0xF3 : 0x0B; terminal_write("Memory");
    terminal_color = is_light_mode ? 0xF0 : 0x0F; terminal_write(": ");
    terminal_color = is_light_mode ? 0xF2 : 0x02; terminal_write(mem_used_buf);
    terminal_write(" KB / ");
    terminal_write(mem_total_buf);
    terminal_write(" MB");
    cursor_y ++;

    terminal_color = is_light_mode ? 0xF0 : 0x0F;
    cursor_x = 0;
    terminal_putc('\n');
}

void terminal_set_light_mode (void) {
    is_light_mode = 1;
    terminal_color = 0xF0;
    terminal_clear();
}

void terminal_readline(char* buf, u32 max) {
    u32 i = 0;
    while (i < max - 1) {
        u8 c = keyboard_getchar();

        if (c == '\n') {
            buf[i] = 0;
            terminal_putc('\n');
            return;
        }
        if (c == '\b' && i) {
            i--;
            if (cursor_x) cursor_x--;
            else { cursor_x = SCREEN_W - 1; cursor_y--; }
            vga_buffer[cursor_y * SCREEN_W + cursor_x] = (terminal_color << 8) | ' ';
        } else if (c >= 32 && c < 127) {
            buf[i++] = c;
            terminal_putc(c);
        }
    }
    buf[max - 1] = 0;
}

void text_editor(void) {
    terminal_clear();
    terminal_color = 0x0B;
    terminal_writeln("=== TEXT EDITOR ===");
    terminal_writeln("i spent way too long on this");
    terminal_writeln("ESC to exit");
    terminal_writeln("");
    terminal_color = 0x0F;

    char lines[20][80];
    for (u32 i = 0; i < 20; i++) {
        mem_set(lines[i], 0, 80);
    }

    u32 line = 0, col = 0;
    cursor_y = 4;
    cursor_x = 0;

    while (1) {
        u8 c = keyboard_getchar();

        if (c == 27) break;

        if (c == '\n') {
            if (line < 19) {
                line++;
                col = 0;
                cursor_x = 0;
                cursor_y++;
                if (cursor_y >= SCREEN_H) {
                    cursor_y = SCREEN_H - 1;
                }
            }
        } else if (c == '\b') {
            if (col > 0) {
                col--;
                lines[line][col] = 0;
                if (cursor_x > 0) cursor_x--;
                vga_buffer[cursor_y * SCREEN_W + cursor_x] = (terminal_color << 8) | ' ';
            }
        } else if (c >= 32 && c < 127 && col < 79) {
            lines[line][col++] = c;
            terminal_putc(c);
        }
    }

    terminal_clear();
}

void paint_app(void) {
    terminal_clear();

    u8 brush = 219;
    u8 color = 0x0E;
    u32 paint_x = 40, paint_y = 12;

    vga_buffer[0] = (0x0B << 8) | '=';
    for (u32 i = 1; i < 30; i++) vga_buffer[i] = (0x0B << 8) | '=';
    vga_buffer[30] = (0x0B << 8) | '=';
    vga_buffer[1] = (0x0B << 8) | ' ';
    vga_buffer[2] = (0x0F << 8) | 'P';
    vga_buffer[3] = (0x0F << 8) | 'A';
    vga_buffer[4] = (0x0F << 8) | 'I';
    vga_buffer[5] = (0x0F << 8) | 'N';
    vga_buffer[6] = (0x0F << 8) | 'T';
    vga_buffer[7] = (0x0B << 8) | ' ';

    vga_buffer[80] = (0x07 << 8) | 'w';
    vga_buffer[81] = (0x07 << 8) | 'h';
    vga_buffer[82] = (0x07 << 8) | 'y';
    vga_buffer[83] = (0x07 << 8) | ' ';
    vga_buffer[84] = (0x07 << 8) | 'd';
    vga_buffer[85] = (0x07 << 8) | 'i';
    vga_buffer[86] = (0x07 << 8) | 'd';
    vga_buffer[87] = (0x07 << 8) | ' ';
    vga_buffer[88] = (0x07 << 8) | 'i';
    vga_buffer[89] = (0x07 << 8) | ' ';
    vga_buffer[90] = (0x07 << 8) | 'm';
    vga_buffer[91] = (0x07 << 8) | 'a';
    vga_buffer[92] = (0x07 << 8) | 'k';
    vga_buffer[93] = (0x07 << 8) | 'e';
    vga_buffer[94] = (0x07 << 8) | ' ';
    vga_buffer[95] = (0x07 << 8) | 't';
    vga_buffer[96] = (0x07 << 8) | 'h';
    vga_buffer[97] = (0x07 << 8) | 'i';
    vga_buffer[98] = (0x07 << 8) | 's';

    vga_buffer[160] = (0x07 << 8) | 'W';
    vga_buffer[161] = (0x07 << 8) | 'A';
    vga_buffer[162] = (0x07 << 8) | 'S';
    vga_buffer[163] = (0x07 << 8) | 'D';
    vga_buffer[164] = (0x07 << 8) | '=';
    vga_buffer[165] = (0x07 << 8) | 'm';
    vga_buffer[166] = (0x07 << 8) | 'o';
    vga_buffer[167] = (0x07 << 8) | 'v';
    vga_buffer[168] = (0x07 << 8) | 'e';
    vga_buffer[170] = (0x07 << 8) | 'S';
    vga_buffer[171] = (0x07 << 8) | 'P';
    vga_buffer[172] = (0x07 << 8) | 'A';
    vga_buffer[173] = (0x07 << 8) | 'C';
    vga_buffer[174] = (0x07 << 8) | 'E';
    vga_buffer[175] = (0x07 << 8) | '=';
    vga_buffer[176] = (0x07 << 8) | 'd';
    vga_buffer[177] = (0x07 << 8) | 'r';
    vga_buffer[178] = (0x07 << 8) | 'a';
    vga_buffer[179] = (0x07 << 8) | 'w';
    vga_buffer[181] = (0x07 << 8) | 'E';
    vga_buffer[182] = (0x07 << 8) | 'S';
    vga_buffer[183] = (0x07 << 8) | 'C';
    vga_buffer[184] = (0x07 << 8) | '=';
    vga_buffer[185] = (0x07 << 8) | 'e';
    vga_buffer[186] = (0x07 << 8) | 'x';
    vga_buffer[187] = (0x07 << 8) | 'i';
    vga_buffer[188] = (0x07 << 8) | 't';

    vga_buffer[paint_y * SCREEN_W + paint_x] = (color << 8) | brush;

    while (1) {
        u8 c = keyboard_getchar();

        if (c == 27) break;

        vga_buffer[paint_y * SCREEN_W + paint_x] = (0x00 << 8) | ' ';

        if (c == 'w' && paint_y > 3) paint_y--;
        if (c == 's' && paint_y < SCREEN_H - 1) paint_y++;
        if (c == 'a' && paint_x > 0) paint_x--;
        if (c == 'd' && paint_x < SCREEN_W - 1) paint_x++;
        if (c == ' ') {
            vga_buffer[paint_y * SCREEN_W + paint_x] = (color << 8) | brush;
        }

        vga_buffer[paint_y * SCREEN_W + paint_x] = (color << 8) | brush;
    }

    terminal_clear();
}

void shell_execute(char* cmd) {
    char* args = cmd;
    while (*args && *args != ' ') args++;
    if (*args) *args++ = 0;

    if (!str_compare(cmd, "help")) {
        terminal_writeln("Available commands:");
        terminal_writeln("  help   - Show this help");
        terminal_writeln("  clear  - Clear screen");
        terminal_writeln("  echo   - Print message");
        terminal_writeln("  pwd    - Show current directory");
        terminal_writeln("  ver    - Show OS version");
        terminal_writeln("  fetch  - System info");
        terminal_writeln("  light  - Set light mode");
        terminal_writeln("  edit   - Text editor");
        terminal_writeln("  paint  - Paint program");
        terminal_writeln("  halt   - Shutdown system");
    } else if (!str_compare(cmd, "clear")) {
        terminal_clear();
    } else if (!str_compare(cmd, "echo")) {
        if (*args) terminal_writeln(args);
    } else if (!str_compare(cmd, "pwd")) {
        terminal_writeln(current_path);
    } else if (!str_compare(cmd, "ver")) {
        terminal_writeln("kill me");
    } else if (!str_compare(cmd, "edit")) {
        text_editor();
    } else if (!str_compare(cmd, "fetch")) {
        breadfetch();
    } else if (!str_compare(cmd, "light")) {
        terminal_set_light_mode();
    } else if (!str_compare(cmd, "paint")) {
        paint_app();
    } else if (!str_compare(cmd, "halt")) {
        terminal_writeln("die");
        asm volatile("cli; hlt");
    } else if (str_len(cmd) > 0) {
        terminal_write("Unknown command: ");
        terminal_writeln(cmd);
    }
}

void shell_loop(void) {
    char buf[256];
    while (1) {
        terminal_color = is_light_mode ? 0xF2 : 0x0A;
        terminal_write("# ");
        terminal_color = is_light_mode ? 0xF0 : 0x0F;
        terminal_readline(buf, sizeof(buf));
        shell_execute(buf);
    }
}

void kernel_main(void) {
    vga_buffer = (u16*)VIDEO_MEM;
    terminal_color = 0x0F;
    terminal_clear();

    idt_init();
    irq_init();

    timer_init(100);

    irq_install_handler(1, keyboard_handler);

    str_copy(current_path, "/");

    asm volatile("sti");

    terminal_color = 0x0E;
    terminal_writeln("bleh");
    terminal_color = 0x07;
    terminal_writeln("meh");
    terminal_writeln("Type 'help' for commands");
    terminal_writeln("");

    shell_loop();

    for (;;) asm volatile("hlt");
}