#include <stdint.h>
#include <stddef.h>
#include <devices/term/tty/tty.h>
#include <lib/klib.h>
#include <devices/dev.h>
#include <lib/lock.h>
#include <sys/apic.h>

#include <misc/vbe.h>
#include <sys/vga_font.h>

#define MAX_TTYS 6
#define KBD_BUF_SIZE 2048
#define BIG_BUF_SIZE 65536
#define MAX_ESC_VALUES 256

static lock_t tty_ready = 0;

struct tty_t {
    lock_t lock;
	int cursor_x;
	int cursor_y;
	int cursor_status;
	uint32_t cursor_bg_col;
	uint32_t cursor_fg_col;
	uint32_t text_bg_col;
	uint32_t text_fg_col;
	char *grid;
	uint32_t *gridbg;
	uint32_t *gridfg;
	int escape;
    int esc_values[MAX_ESC_VALUES];
    int esc_values_i;
    int rrr;
	int tabsize;
    lock_t kbd_event;
    lock_t kbd_lock;
    size_t kbd_buf_i;
    char kbd_buf[KBD_BUF_SIZE];
    size_t big_buf_i;
    char big_buf[BIG_BUF_SIZE];
    struct termios_t termios;
};

/*
static uint32_t ansi_colours[] = {
    0x00000000,              // black
    0x00aa0000,              // red
    0x0000aa00,              // green
    0x00aa5500,              // brown
    0x000000aa,              // blue
    0x00aa00aa,              // magenta
    0x0000aaaa,              // cyan
    0x00aaaaaa               // grey
};
*/

// Solarized-like scheme
static uint32_t ansi_colours[] = {
    0x00073642,              // black
    0x00dc322f,              // red
    0x00859900,              // green
    0x00b58900,              // brown
    0x00268bd2,              // blue
    0x00d33682,              // magenta
    0x002aa198,              // cyan
    0x00eee8d5              // grey
};

static struct tty_t ttys[MAX_TTYS];
static int current_tty = 0;

static const char *tty_names[MAX_TTYS] = {
    "tty0", "tty1", "tty2", "tty3", "tty4", "tty5"
};

#include "output.inc"
#include "input.inc"

static int tty_flush(int tty) {
    (void)tty;
    return 1;
}

// TODO
static int tty_tcsetattr(int tty, int optional_actions, struct termios_t *new_termios) {
    return 0;
}

void init_tty_extended(uint32_t *__fb,
              int __fb_height,
              int __fb_width,
              int __fb_pitch,
              uint8_t *__font,
              int __font_height,
              int __font_width) {
    kprint(KPRN_INFO, "tty: Initialising...");

    fb = __fb;
    fb_height = __fb_height;
    fb_width = __fb_width;
    fb_pitch = __fb_pitch;
    font = __font;
    font_height = __font_height;
    font_width = __font_width;

    cols = fb_width / font_width;
    rows = fb_height / font_height;

    for (int i = 0; i < MAX_TTYS; i++) {
        ttys[i].lock = 1;
	    ttys[i].cursor_x = 0;
	    ttys[i].cursor_y = 0;
	    ttys[i].cursor_status = 1;
	    ttys[i].cursor_bg_col = 0x00ffffff;
	    ttys[i].cursor_fg_col = 0x00000000;
	    ttys[i].text_bg_col = ansi_colours[0];
	    ttys[i].text_fg_col = ansi_colours[7];
	    ttys[i].escape = 0;
	    ttys[i].tabsize = 8;
        ttys[i].kbd_event = 0;
        ttys[i].kbd_lock = 1;
        ttys[i].kbd_buf_i = 0;
        ttys[i].big_buf_i = 0;
        ttys[i].termios.c_lflag = (ICANON | ECHO);
        ttys[i].grid = kalloc(rows * cols);
        ttys[i].gridbg = kalloc(rows * cols * sizeof(uint32_t));
        ttys[i].gridfg = kalloc(rows * cols * sizeof(uint32_t));
        if (!ttys[i].grid || !ttys[i].gridbg || !ttys[i].gridfg)
            panic("Out of memory", 0, 0);
        for (size_t j = 0; j < (size_t)(rows * cols); j++) {
            ttys[i].grid[j] = ' ';
            ttys[i].gridbg[j] = ttys[i].text_bg_col;
            ttys[i].gridfg[j] = ttys[i].text_fg_col;
        }
        refresh(i);
        struct device_t device = {0};
        kstrcpy(device.name, tty_names[i]);
        device.intern_fd = i;
        device.size = 0;
        device.calls.read = tty_read;
        device.calls.write = tty_write;
        device.calls.flush = tty_flush;
        device_add(&device);
    }

    io_apic_set_mask(0, 1, 1);
    task_tcreate(0, tcreate_fn_call, tcreate_fn_call_data(kbd_handler, 0));

    spinlock_release(&tty_ready);
    kprint(KPRN_INFO, "tty: Ready!");
    return;
}

void init_tty(void) {
    init_tty_extended(
        vbe_framebuffer,
        vbe_height,
        vbe_width,
        vbe_pitch,
        vga_font,
        vga_font_height,
        vga_font_width);
}