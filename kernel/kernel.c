#include "types.h"

#define VIDEO_MEM 0xB8000
#define SCREEN_W  80
#define SCREEN_H  25

extern void idt_init(void);
extern void irq_init(void);
extern void irq_install_handler(int irq, void (*handler)(void));
extern void net_init(void);
extern void net_poll(void);
extern void net_show_ip(void);
extern void icmp_send_ping(u32 ip);
extern u32 str_to_ip(const char* str);
extern void ip_to_str(u32 ip, char* buf);
extern u32 http_request(u32 server_ip, u16 port, const char* path, u8* response, u32 max_len);

extern void fs_init(void);
extern int fs_create(const char* path, u8 is_directory);
extern int fs_write(const char* path, const u8* data, u32 size);
extern int fs_read(const char* path, u8* data, u32 max_size);
extern int fs_delete(const char* path);
extern int fs_list(const char* path, char* buffer, u32 max_size);
extern int fs_exists(const char* path);
extern u32 fs_size(const char* path);

static u16* vga_buffer;
static u32  cursor_y, cursor_x;
static u8   terminal_color;
static char current_path[256];

void terminal_write(const char* s);
void terminal_writeln(const char* s);
void terminal_readline(char* buf, u32 max);

int str_compare(const char* a, const char* b);
void str_copy(char* dest, const char* src);
void mem_set(void* dest, u8 val, u32 len);
u32 str_len(const char* s);

void construct_path(char* dest, const char* current_path, const char* arg) {
    if (arg[0] == '/') {
        str_copy(dest, arg);
    } else if (!str_compare(arg, "..")) {
        str_copy(dest, current_path);
        u32 len = str_len(dest);
        if (len > 1) {
            if (dest[len - 1] == '/') len--;
            while (len > 1 && dest[len - 1] != '/') len--;
            dest[len] = '/';
            dest[len + 1] = 0;
        } else {
            str_copy(dest, "/");
        }
    } else if (!str_compare(arg, ".")) {
        str_copy(dest, current_path);
    } else {
        str_copy(dest, current_path);
        if (current_path[str_len(current_path) - 1] != '/') {
            u32 len = str_len(dest);
            dest[len] = '/';
            dest[len + 1] = 0;
        }
        u32 len = str_len(dest);
        str_copy(dest + len, arg);
    }
}

#define MAX_USERS 16
#define MAX_USERNAME 32
#define MAX_PASSWORD 32

typedef struct {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    u8 is_active;
    u32 uid;
    u8 is_admin;
} user_t;

static user_t users[MAX_USERS];
static u32 current_user_id = (u32)-1;
static u32 next_uid = 1;

static void user_init(void) {
    for (u32 i = 0; i < MAX_USERS; i++) {
        mem_set(users[i].username, 0, MAX_USERNAME);
        mem_set(users[i].password, 0, MAX_PASSWORD);
        users[i].is_active = 0;
        users[i].uid = 0;
        users[i].is_admin = 0;
    }
    current_user_id = (u32)-1;
}

static int user_login(const char* username, const char* password) {
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].is_active && !str_compare(users[i].username, username)) {
            if (!str_compare(users[i].password, password)) {
                current_user_id = users[i].uid;
                return 0;
            }
            return -1;
        }
    }
    return -1;
}

static void user_logout(void) {
    current_user_id = (u32)-1;
}

static int root_exists(void) {
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].is_active && !str_compare(users[i].username, "root")) return 1;
    }
    return 0;
}

static u8 is_admin(void) {
    if (current_user_id == (u32)-1) return 0;
    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].uid == current_user_id && users[i].is_active) {
            return users[i].is_admin;
        }
    }
    return 0;
}

static void setup_root_user(void) {
    if (root_exists()) return;

    terminal_color = 0x0E;
    terminal_writeln("=== First boot setup ===");
    terminal_color = 0x07;
    terminal_writeln("Create the root user (required).");
    terminal_writeln("");

    char pw[MAX_PASSWORD];
    while (1) {
        terminal_write("root password: ");
        terminal_readline(pw, sizeof(pw));

        if (str_len(pw) == 0) {
            terminal_writeln("password cannot be empty");
            continue;
        }
        break;
    }

    str_copy(users[0].username, "root");
    str_copy(users[0].password, pw);
    users[0].is_active = 1;
    users[0].uid = 0;
    users[0].is_admin = 1;
    current_user_id = (u32)-1;

    terminal_writeln("");
    terminal_writeln("root created. Please login: login root <password>");
}

static int user_create(const char* username, const char* password, u8 admin) {
    if (str_len(username) == 0 || str_len(password) == 0) return -3;

    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].is_active && !str_compare(users[i].username, username)) {
            return -1;
        }
    }

    for (u32 i = 0; i < MAX_USERS; i++) {
        if (!users[i].is_active) {
            str_copy(users[i].username, username);
            str_copy(users[i].password, password);
            users[i].is_active = 1;
            users[i].uid = next_uid++;
            users[i].is_admin = admin;
            return 0;
        }
    }
    return -2;
}

static int user_delete(const char* username) {
    if (!str_compare(username, "root")) return -1;

    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].is_active && !str_compare(users[i].username, username)) {
            if (users[i].uid == current_user_id) return -2;
            users[i].is_active = 0;
            return 0;
        }
    }
    return -3;
}

static int user_passwd(const char* username, const char* new_password) {
    if (str_len(new_password) == 0) return -1;

    for (u32 i = 0; i < MAX_USERS; i++) {
        if (users[i].is_active && !str_compare(users[i].username, username)) {
            str_copy(users[i].password, new_password);
            return 0;
        }
    }
    return -2;
}

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
    __asm__ __volatile__(
        "outb %b0, %w1"
        :
        : "a"((u8)val), "Nd"((u16)port)
    );
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ __volatile__(
        "inb %w1, %b0"
        : "=a"(ret)
        : "Nd"((u16)port)
    );
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

    static u8 ctrl_pressed = 0;
    if (sc == 0x1D) ctrl_pressed = 1;
    else if (sc == 0x9D) ctrl_pressed = 0;

    if (ctrl_pressed && sc == 0x2E) {
        u32 next = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
        if (next != kbd_read_pos) {
            kbd_buffer[kbd_write_pos] = 0x03;
            kbd_write_pos = next;
        }
    }
}

u8 keyboard_getchar(void) {
    while (kbd_read_pos == kbd_write_pos) {
        asm volatile("hlt");
        net_poll();
    }
    u8 c = kbd_buffer[kbd_read_pos];
    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
    return c;
}

static void disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
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
        terminal_writeln("  help     - Show this help");
        terminal_writeln("  clear    - Clear screen");
        terminal_writeln("  echo     - Print message");
        terminal_writeln("  ls       - List files");
        terminal_writeln("  cat      - Show file contents");
        terminal_writeln("  touch    - Create file");
        terminal_writeln("  mkdir    - Create directory");
        terminal_writeln("  rm       - Delete file");
        terminal_writeln("  write    - Write to file");
        terminal_writeln("  pwd      - Show current directory");
        terminal_writeln("  cd       - Change directory");
        terminal_writeln("  ver      - Show OS version");
        terminal_writeln("  edit     - Text editor");
        terminal_writeln("  paint    - Paint program");
        terminal_writeln("  ifconfig - Show network info");
        terminal_writeln("  ping     - Ping IP address");
        terminal_writeln("  useradd  - Create new user (admin only)");
        terminal_writeln("  userdel  - Delete user (admin only)");
        terminal_writeln("  passwd   - Change password");
        terminal_writeln("  users    - List all users (admin only)");
        terminal_writeln("  login    - Login as user");
        terminal_writeln("  logout   - Logout current user");
        terminal_writeln("  whoami   - Show current user");
        terminal_writeln("  exec     - Run package");
        terminal_writeln("  pkg      - Package manager");
        terminal_writeln("            pkg list [server_ip]           - List packages (port 40000)");
        terminal_writeln("            pkg install <server_ip> <pkg>  - Install package (port 40000)");
        terminal_writeln("            pkg test <server_ip>           - Test connectivity");
        terminal_writeln("  halt     - Shutdown system");
    } else if (!str_compare(cmd, "clear")) {
        terminal_clear();
    } else if (!str_compare(cmd, "echo")) {
        if (*args) terminal_writeln(args);
    } else if (!str_compare(cmd, "pwd")) {
        terminal_writeln(current_path);
    } else if (!str_compare(cmd, "cd")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);

            if (path[str_len(path) - 1] != '/') {
                u32 len = str_len(path);
                path[len] = '/';
                path[len + 1] = 0;
            }

            if (fs_exists(path) && fs_size(path) == 0) {
                str_copy(current_path, path);
            } else {
                terminal_writeln("directory not found");
            }
        } else {
            str_copy(current_path, "/");
        }
    } else if (!str_compare(cmd, "ver")) {
        terminal_writeln("UwU OS v1.0.0");
    } else if (!str_compare(cmd, "edit")) {
        text_editor();
    } else if (!str_compare(cmd, "paint")) {
        paint_app();
    } else if (!str_compare(cmd, "ifconfig")) {
        net_show_ip();
    } else if (!str_compare(cmd, "ls")) {
        char buffer[1024];
        int result = fs_list(current_path, buffer, sizeof(buffer));
        if (result > 0) {
            terminal_write(buffer);
        } else {
            terminal_writeln("empty or error");
        }
    } else if (!str_compare(cmd, "cat")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);

            u8 buffer[4096];
            int size = fs_read(path, buffer, sizeof(buffer));
            if (size > 0) {
                for (int i = 0; i < size; i++) {
                    terminal_putc(buffer[i]);
                }
                if (buffer[size - 1] != '\n') {
                    terminal_putc('\n');
                }
            } else {
                terminal_writeln("file not found or error");
            }
        } else {
            terminal_writeln("usage: cat <file>");
        }
    } else if (!str_compare(cmd, "touch")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);

            if (fs_create(path, 0) == 0) {
                terminal_writeln("file created");
            } else {
                terminal_writeln("error creating file");
            }
        } else {
            terminal_writeln("usage: touch <file>");
        }
    } else if (!str_compare(cmd, "mkdir")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);

            if (fs_create(path, 1) == 0) {
                terminal_writeln("directory created");
            } else {
                terminal_writeln("error creating directory");
            }
        } else {
            terminal_writeln("usage: mkdir <dir>");
        }
    } else if (!str_compare(cmd, "rm")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);

            if (fs_delete(path) == 0) {
                terminal_writeln("deleted");
            } else {
                terminal_writeln("file not found");
            }
        } else {
            terminal_writeln("usage: rm <file>");
        }
    } else if (!str_compare(cmd, "write")) {
        if (*args) {
            char* space = args;
            while (*space && *space != ' ') space++;
            if (*space) {
                *space = 0;
                space++;

                char path[256];
                construct_path(path, current_path, args);

                int result = fs_write(path, (u8*)space, str_len(space));
                if (result > 0) {
                    terminal_writeln("written");
                } else {
                    terminal_writeln("write error");
                }
            } else {
                terminal_writeln("usage: write <file> <data>");
            }
        } else {
            terminal_writeln("usage: write <file> <data>");
        }
    } else if (!str_compare(cmd, "ping")) {
        if (*args) {
            u32 ip = str_to_ip(args);
            terminal_write("PING ");
            char buf[16];
            ip_to_str(ip, buf);
            terminal_write(buf);
            terminal_writeln(" (press Ctrl+C to stop)");

            kbd_read_pos = kbd_write_pos;

            u32 count = 0;
            u8 running = 1;

            while (running) {
                for (u32 tries = 0; tries < 2000; tries++) {
                    net_poll();
                    for (volatile int i = 0; i < 100; i++);
                }

                icmp_send_ping(ip);
                count++;

                u32 timeout = 1000000;
                while (timeout-- > 0) {
                    net_poll();

                    if (kbd_read_pos != kbd_write_pos) {
                        u8 c = kbd_buffer[kbd_read_pos];
                        if (c == 0x03) {
                            kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
                            running = 0;
                            terminal_writeln("^C");
                            break;
                        }
                        kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
                    }

                    for (volatile int i = 0; i < 100; i++);
                }

                if (!running) break;

                for (volatile int i = 0; i < 500000; i++) {
                    if (kbd_read_pos != kbd_write_pos) {
                        u8 c = kbd_buffer[kbd_read_pos];
                        if (c == 0x03) {
                            kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
                            running = 0;
                            terminal_writeln("^C");
                            break;
                        }
                        kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;
                    }
                }
            }

            terminal_writeln("");
            terminal_write("--- ");
            terminal_write(buf);
            terminal_writeln(" ping statistics ---");

            char countbuf[12];
            int pos = 0;
            u32 temp = count;
            if (temp == 0) {
                countbuf[pos++] = '0';
            } else {
                char digits[12];
                int dpos = 0;
                while (temp > 0) {
                    digits[dpos++] = '0' + (temp % 10);
                    temp /= 10;
                }
                while (dpos > 0) {
                    countbuf[pos++] = digits[--dpos];
                }
            }
            countbuf[pos] = 0;

            terminal_write(countbuf);
            terminal_writeln(" packets transmitted");
        } else {
            terminal_writeln("usage: ping <ip>");
        }
    } else if (!str_compare(cmd, "useradd")) {
        if (!is_admin()) {
            terminal_writeln("permission denied: admin only");
        } else if (*args) {
            char* space = args;
            while (*space && *space != ' ') space++;
            if (*space) {
                *space = 0;
                space++;

                char* admin_flag = space;
                while (*admin_flag && *admin_flag != ' ') admin_flag++;
                u8 make_admin = 0;
                if (*admin_flag) {
                    *admin_flag = 0;
                    admin_flag++;
                    if (!str_compare(admin_flag, "admin")) {
                        make_admin = 1;
                    }
                }

                int result = user_create(args, space, make_admin);
                if (result == 0) {
                    terminal_writeln("user created");
                } else if (result == -1) {
                    terminal_writeln("user already exists");
                } else if (result == -2) {
                    terminal_writeln("user table full");
                } else {
                    terminal_writeln("invalid username or password");
                }
            } else {
                terminal_writeln("usage: useradd <username> <password> [admin]");
            }
        } else {
            terminal_writeln("usage: useradd <username> <password> [admin]");
        }
    } else if (!str_compare(cmd, "userdel")) {
        if (!is_admin()) {
            terminal_writeln("permission denied: admin only");
        } else if (*args) {
            int result = user_delete(args);
            if (result == 0) {
                terminal_writeln("user deleted");
            } else if (result == -1) {
                terminal_writeln("cannot delete root user");
            } else if (result == -2) {
                terminal_writeln("cannot delete currently logged in user");
            } else {
                terminal_writeln("user not found");
            }
        } else {
            terminal_writeln("usage: userdel <username>");
        }
    } else if (!str_compare(cmd, "passwd")) {
        if (*args) {
            char* space = args;
            while (*space && *space != ' ') space++;

            if (is_admin() && *space) {
                *space = 0;
                space++;
                int result = user_passwd(args, space);
                if (result == 0) {
                    terminal_writeln("password changed");
                } else if (result == -1) {
                    terminal_writeln("password cannot be empty");
                } else {
                    terminal_writeln("user not found");
                }
            } else if (!is_admin()) {
                for (u32 i = 0; i < MAX_USERS; i++) {
                    if (users[i].uid == current_user_id && users[i].is_active) {
                        int result = user_passwd(users[i].username, args);
                        if (result == 0) {
                            terminal_writeln("password changed");
                        } else {
                            terminal_writeln("password cannot be empty");
                        }
                        break;
                    }
                }
            } else {
                terminal_writeln("usage: passwd <new_password> or passwd <username> <new_password> (admin)");
            }
        } else {
            terminal_writeln("usage: passwd <new_password> or passwd <username> <new_password> (admin)");
        }
    } else if (!str_compare(cmd, "users")) {
        if (!is_admin()) {
            terminal_writeln("permission denied: admin only");
        } else {
            terminal_writeln("Users:");
            for (u32 i = 0; i < MAX_USERS; i++) {
                if (users[i].is_active) {
                    terminal_write("  ");
                    terminal_write(users[i].username);
                    if (users[i].is_admin) {
                        terminal_write(" (admin)");
                    }
                    terminal_writeln("");
                }
            }
        }
    } else if (!str_compare(cmd, "login")) {
        if (*args) {
            char* space = args;
            while (*space && *space != ' ') space++;
            if (*space) {
                *space = 0;
                space++;

                if (user_login(args, space) == 0) {
                    terminal_write("logged in as: ");
                    terminal_writeln(args);
                } else {
                    terminal_writeln("login failed");
                }
            } else {
                terminal_writeln("usage: login <username> <password>");
            }
        } else {
            terminal_writeln("usage: login <username> <password>");
        }
    } else if (!str_compare(cmd, "logout")) {
        user_logout();
        terminal_writeln("logged out");
    } else if (!str_compare(cmd, "whoami")) {
        if (current_user_id == (u32)-1) {
            terminal_writeln("not logged in");
        } else {
            for (u32 i = 0; i < MAX_USERS; i++) {
                if (users[i].uid == current_user_id && users[i].is_active) {
                    terminal_write(users[i].username);
                    if (users[i].is_admin) {
                        terminal_write(" (admin)");
                    }
                    terminal_writeln("");
                    break;
                }
            }
        }
    } else if (!str_compare(cmd, "pkg")) {
        if (*args) {
            char* space = args;
            while (*space && *space != ' ') space++;
            if (*space) *space++ = 0;

            if (!str_compare(args, "install")) {
                if (*space) {
                    char* space2 = space;
                    while (*space2 && *space2 != ' ') space2++;
                    if (*space2) *space2++ = 0;

                    u32 server_ip = str_to_ip(space);
                    char path[256];
                    str_copy(path, "/packages/");
                    u32 path_len = str_len(path);
                    str_copy(path + path_len, space2);

                    terminal_write("Downloading package from ");
                    terminal_write(space);
                    terminal_write("... ");

	                    u8 response[8192];
                    u32 response_len = http_request(server_ip, 40000, path, response, sizeof(response));

                    if (response_len > 0) {
                        u8 is_ok = 0;
                        if (response_len >= 12) {
                            if (response[9] == '2' && response[10] == '0' && response[11] == '0') {
                                is_ok = 1;
                            }
                        }

                        if (!is_ok) {
                            terminal_writeln("failed (server error or package not found)");
                        } else {
                            char pkg_path[256];
                            str_copy(pkg_path, "/packages/");
                            u32 pkg_path_len = str_len(pkg_path);
                            str_copy(pkg_path + pkg_path_len, space2);

                            u32 body_start = 0;
                            u32 content_length = 0;

                            for (u32 i = 0; i < response_len - 3; i++) {
                                if (response[i] == '\r' && response[i+1] == '\n' &&
                                    response[i+2] == '\r' && response[i+3] == '\n') {
                                    body_start = i + 4;
                                    break;
                                }
                            }

                            if (body_start >= 16) {
                                for (u32 i = 0; i < body_start - 15; i++) {
                                    if (response[i] == 'C' && response[i+1] == 'o' &&
                                        response[i+2] == 'n' && response[i+3] == 't' &&
                                        response[i+4] == 'e' && response[i+5] == 'n' &&
                                        response[i+6] == 't' && response[i+7] == '-' &&
                                        response[i+8] == 'L' && response[i+9] == 'e' &&
                                        response[i+10] == 'n' && response[i+11] == 'g' &&
                                        response[i+12] == 't' && response[i+13] == 'h') {
                                        u32 j = i + 14;
                                        while (j < body_start && (response[j] == ' ' || response[j] == ':')) j++;
                                        while (j < body_start && response[j] >= '0' && response[j] <= '9') {
                                            content_length = content_length * 10 + (response[j] - '0');
                                            j++;
                                        }
                                        break;
                                    }
                                }
                            }

                            u32 body_len;
                            if (body_start > 0 && body_start < response_len) {
                                if (content_length > 0) {
                                    body_len = content_length;
                                } else {
                                    body_len = response_len - body_start;
                                }
                                u32 body_end = body_start + body_len;
                                if (body_end > response_len) body_end = response_len;
                                
                                fs_write(pkg_path, response + body_start, body_end - body_start);
                                terminal_writeln("done");
                            } else {
                                fs_write(pkg_path, response, response_len);
                                terminal_writeln("done");
                            }
                        }
                    } else {
                        terminal_writeln("failed (connection timeout or server unreachable)");
                    }
                } else {
                    terminal_writeln("usage: pkg install <server_ip> <package_name>");
                }
            } else if (!str_compare(args, "list")) {
                if (*space) {
                    u32 server_ip = str_to_ip(space);

	                    u8 response[8192];
                    u32 response_len = http_request(server_ip, 40000, "/packages/list", response, sizeof(response));

                    if (response_len > 0) {
                        u8 is_ok = 0;
                        if (response_len >= 12) {
                            if (response[9] == '2' && response[10] == '0' && response[11] == '0') {
                                is_ok = 1;
                            }
                        }

                        if (!is_ok) {
                            terminal_writeln("failed (server error)");
                        } else {
                            u32 body_start = 0;
                            u32 content_length = 0;

                            for (u32 i = 0; i < response_len - 3; i++) {
                                if (response[i] == '\r' && response[i+1] == '\n' &&
                                    response[i+2] == '\r' && response[i+3] == '\n') {
                                    body_start = i + 4;
                                    break;
                                }
                            }

                            if (body_start >= 16) {
                                for (u32 i = 0; i < body_start - 15; i++) {
                                    if (response[i] == 'C' && response[i+1] == 'o' &&
                                        response[i+2] == 'n' && response[i+3] == 't' &&
                                        response[i+4] == 'e' && response[i+5] == 'n' &&
                                        response[i+6] == 't' && response[i+7] == '-' &&
                                        response[i+8] == 'L' && response[i+9] == 'e' &&
                                        response[i+10] == 'n' && response[i+11] == 'g' &&
                                        response[i+12] == 't' && response[i+13] == 'h') {
                                        u32 j = i + 14;
                                        while (j < body_start && (response[j] == ' ' || response[j] == ':')) j++;
                                        while (j < body_start && response[j] >= '0' && response[j] <= '9') {
                                            content_length = content_length * 10 + (response[j] - '0');
                                            j++;
                                        }
                                        break;
                                    }
                                }
                            }

                            if (body_start > 0 && body_start < response_len) {
                                u32 body_len;
                                if (content_length > 0) {
                                    body_len = content_length;
                                } else {
                                    body_len = response_len - body_start;
                                }
                                if (body_start + body_len > response_len) {
                                    body_len = response_len - body_start;
                                }

                                u32 body_end = body_start + body_len;
                                if (body_end > response_len) body_end = response_len;
                                
                                if (body_end < sizeof(response)) {
                                    response[body_end] = 0;
                                } else {
                                    response[sizeof(response) - 1] = 0;
                                }
                                terminal_writeln("");
                                terminal_write((char*)(response + body_start));
                                terminal_writeln("");
                            } else {
                                terminal_writeln("failed to parse response");
                            }
                        }
                    } else {
                        terminal_writeln("failed (connection timeout or server unreachable)");
                    }
                } else {
                    char buffer[1024];
                    int result = fs_list("/packages/", buffer, sizeof(buffer));
                    if (result > 0) {
                        terminal_writeln("Installed packages:");
                        terminal_write(buffer);
                    } else {
                        terminal_writeln("no packages installed");
                    }
                }
            } else if (!str_compare(args, "test")) {
                if (*space) {
                    u32 server_ip = str_to_ip(space);
                    terminal_write("Testing connectivity to ");
                    terminal_write(space);
                    terminal_write("... ");
                    icmp_send_ping(server_ip);
                    terminal_writeln("Ping sent! Check for ping replies above.");
                } else {
                    terminal_writeln("usage: pkg test <server_ip>");
                }
            } else {
                terminal_writeln("usage: pkg <install|list|test> [args]");
            }
        } else {
            terminal_writeln("usage: pkg <install|list|test> [args]");
        }
    } else if (!str_compare(cmd, "exec")) {
        if (*args) {
            char path[256];
            construct_path(path, current_path, args);
            u8 buffer[4096];
            int size = fs_read(path, buffer, sizeof(buffer));
            if (size > 0) {
                buffer[size] = 0;
                terminal_write((char*)buffer);
            } else {
                terminal_writeln("file not found");
            }
        } else {
            terminal_writeln("usage: exec <file>");
        }
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
        terminal_color = 0x0A;
        terminal_write("# ");
        terminal_color = 0x0F;
        terminal_readline(buf, sizeof(buf));
        shell_execute(buf);
    }
}

void kernel_main(void) {
    vga_buffer = (u16*)VIDEO_MEM;
    terminal_color = 0x0F;
    disable_cursor();
    terminal_clear();

    idt_init();
    irq_init();

    irq_install_handler(1, keyboard_handler);

    str_copy(current_path, "/");

    user_init();

    asm volatile("sti");

    terminal_color = 0x0E;
    terminal_writeln("UwU OS starting...");
    terminal_color = 0x07;

    terminal_write("initializing filesystem... ");
    fs_init();
    terminal_writeln("done");

    fs_create("/packages/", 1);

    terminal_write("initializing network... ");
    net_init();
    terminal_writeln("done (maybe)");

    setup_root_user();

    terminal_writeln("Type 'help' for commands");
    terminal_writeln("");

    shell_loop();

    for (;;) asm volatile("hlt");
}