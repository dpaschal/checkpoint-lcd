/*
 * sdeclcd.c — SDEC LCD driver implementation
 *
 * Reverse-engineered from Check Point / OPNsense sdeclcd.so binary.
 *
 * Hardware: HD44780 20x2 character LCD on LPT1 parallel port.
 *
 * LPT control port bit mapping (bits 0,1,3 are hardware-inverted):
 *   bit 0 (~Strobe, inverted)  → Backlight (write 0 = pin HIGH = BL on)
 *   bit 1 (~AutoFeed, inverted) → E Enable (write 0 = pin HIGH = E HIGH)
 *   bit 2 (Init, not inverted) → unused
 *   bit 3 (~SelectIn, inverted) → RS (write 1 = pin LOW = command mode)
 *
 * Strobe sequence from disassembly:
 *   Command (RS=0): ctrl = BL ^ 0x09, data out, nsleep, ctrl ^= 0x0B
 *     - 0x09 = bits 0,3 → E HIGH (bit1=0), RS LOW (bit3=1 inverted)
 *     - XOR 0x0B flips bit1 → E goes LOW (falling edge latches)
 *
 *   Data (RS=1): ctrl = !BL, data out, nsleep, ctrl ^= 0x03
 *     - !BL with no bit3 → E HIGH (bit1=0), RS HIGH (bit3=0 inverted)
 *     - XOR 0x03 flips bit1 → E goes LOW (falling edge latches)
 */

#include "sdeclcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#else
#include <sys/io.h>
#endif

/* ── I/O port access ───────────────────────────────────────────────── */

static inline void port_out(uint16_t port, uint8_t val)
{
#ifdef __FreeBSD__
    outb(port, val);
#else
    outb(val, port);
#endif
}

static inline uint8_t port_in(uint16_t port)
{
    return inb(port);
}

static void nsleep(long ns)
{
    struct timespec ts = { .tv_sec = 0, .tv_nsec = ns };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

/* ── Low-level strobe — exact match to sdeclcd.so disassembly ──────── */

/*
 * Backlight state as the original tracks it:
 *   bl = 1 when backlight is on, 0 when off.
 *   This maps to ctrl bit 0: write !bl (because bit0 is inverted).
 */

/*
 * Send HD44780 COMMAND byte (RS=0).
 *
 * From disassembly (sdeclcd_flush, address set):
 *   r15b = (backlight != 0) ? 1 : 0
 *   al = r15b ^ 0x09     → out 0x37a  (E HIGH via bit1=0, RS LOW via bit3=1)
 *   data byte             → out 0x378
 *   nanosleep
 *   r15b ^= 0x0B; al=r15b → out 0x37a  (E LOW via bit1=1, falling edge latches)
 *   nanosleep
 */
void sdec_cmd(sdec_t *ctx, uint8_t cmd)
{
    uint8_t bl = ctx->backlight ? 1 : 0;
    uint8_t c;

    /* Phase 1: E HIGH, RS LOW (command mode) */
    c = bl ^ 0x09;
    port_out(SDEC_PORT_CTRL, c);

    /* Write command byte to data port */
    port_out(SDEC_PORT_DATA, cmd);
    nsleep(2000);

    /* Phase 2: E LOW (falling edge latches the command) */
    c = bl ^ 0x09 ^ 0x0B;   /* = bl ^ 0x02 */
    port_out(SDEC_PORT_CTRL, c);
    nsleep(2000);

    /* Extra settle time for slow commands */
    if (cmd <= 0x03)   /* clear display, cursor home */
        usleep(2000);
    else
        usleep(50);
}

/*
 * Send HD44780 DATA byte (RS=1).
 *
 * From disassembly (sdeclcd_flush, character write):
 *   al = (backlight == 0) ? 1 : 0   (sete, INVERTED from r15b)
 *   out 0x37a            (E HIGH via bit1=0, RS HIGH via bit3=0)
 *   data byte → out 0x378
 *   nanosleep
 *   r15b ^= 0x03; al=r15b → out 0x37a  (E LOW via bit1=1)
 *   nanosleep
 */
void sdec_data(sdec_t *ctx, uint8_t data)
{
    uint8_t bl = ctx->backlight ? 1 : 0;
    uint8_t c;

    /* Phase 1: E HIGH, RS HIGH (data mode) */
    c = bl ? 0 : 1;   /* !bl — matches sete instruction */
    port_out(SDEC_PORT_CTRL, c);

    /* Write data byte */
    port_out(SDEC_PORT_DATA, data);
    nsleep(2000);

    /* Phase 2: E LOW (falling edge latches the data) */
    c = bl ^ 0x03;    /* matches r15b ^= 0x03 from disasm */
    port_out(SDEC_PORT_CTRL, c);
    nsleep(2000);

    usleep(50);
}

uint8_t sdec_status(sdec_t *ctx)
{
    (void)ctx;
    return port_in(SDEC_PORT_STATUS) & SDEC_BTN_MASK;
}

/* ── Core API ──────────────────────────────────────────────────────── */

int sdec_open(sdec_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->backlight = true;
    ctx->io_fd = -1;

#ifdef __FreeBSD__
    ctx->io_fd = open("/dev/io", O_RDWR);
    if (ctx->io_fd < 0) {
        fprintf(stderr, "sdec: cannot open /dev/io: %s (need root)\n",
                strerror(errno));
        return -1;
    }
#else
    if (ioperm(SDEC_PORT_DATA, 3, 1) != 0) {
        fprintf(stderr, "sdec: ioperm failed: %s (need root)\n",
                strerror(errno));
        return -1;
    }
#endif

    ctx->opened = true;
    return 0;
}

void sdec_close(sdec_t *ctx)
{
    if (!ctx->opened) return;
#ifdef __FreeBSD__
    if (ctx->io_fd >= 0) { close(ctx->io_fd); ctx->io_fd = -1; }
#else
    ioperm(SDEC_PORT_DATA, 3, 0);
#endif
    ctx->opened = false;
}

int sdec_init(sdec_t *ctx)
{
    if (!ctx->opened) return -1;

    /* HD44780 power-on init sequence per datasheet */
    usleep(50000);              /* wait >40ms after power-on */

    sdec_cmd(ctx, 0x38);        /* function set: 8-bit, 2 lines, 5x8 */
    usleep(5000);               /* wait >4.1ms */
    sdec_cmd(ctx, 0x38);        /* repeat */
    usleep(200);                /* wait >100us */
    sdec_cmd(ctx, 0x38);        /* third time */

    sdec_cmd(ctx, 0x0C);        /* display ON, cursor OFF, blink OFF */
    sdec_cmd(ctx, 0x01);        /* clear display */
    sdec_cmd(ctx, 0x06);        /* entry mode: increment, no shift */

    /* Initialize buffers */
    memset(ctx->buf, ' ', sizeof(ctx->buf));
    memset(ctx->prev, ' ', sizeof(ctx->prev));
    for (int r = 0; r < SDEC_ROWS; r++) {
        ctx->buf[r][SDEC_COLS] = '\0';
        ctx->prev[r][SDEC_COLS] = '\0';
    }

    return 0;
}

/* ── Display operations ────────────────────────────────────────────── */

int sdec_clear(sdec_t *ctx)
{
    memset(ctx->buf, ' ', sizeof(ctx->buf));
    for (int r = 0; r < SDEC_ROWS; r++)
        ctx->buf[r][SDEC_COLS] = '\0';
    return 0;
}

int sdec_puts(sdec_t *ctx, int row, const char *text)
{
    if (row < 0 || row >= SDEC_ROWS) return -1;
    int len = strlen(text);
    if (len > SDEC_COLS) len = SDEC_COLS;
    memset(ctx->buf[row], ' ', SDEC_COLS);
    memcpy(ctx->buf[row], text, len);
    ctx->buf[row][SDEC_COLS] = '\0';
    return 0;
}

int sdec_printf(sdec_t *ctx, int row, const char *fmt, ...)
{
    char tmp[SDEC_COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    return sdec_puts(ctx, row, tmp);
}

int sdec_putc(sdec_t *ctx, int row, int col, char c)
{
    if (row < 0 || row >= SDEC_ROWS) return -1;
    if (col < 0 || col >= SDEC_COLS) return -1;
    ctx->buf[row][col] = c;
    return 0;
}

/*
 * Flush — send only changed characters.
 * Uses sdec_cmd for DDRAM address, sdec_data for characters.
 */
int sdec_flush(sdec_t *ctx)
{
    for (int row = 0; row < SDEC_ROWS; row++) {
        bool need_addr = true;
        for (int col = 0; col < SDEC_COLS; col++) {
            if (ctx->buf[row][col] != ctx->prev[row][col]) {
                if (need_addr) {
                    uint8_t addr = (row == 0) ? 0x80 : 0xC0;
                    addr += col;
                    sdec_cmd(ctx, addr);
                    need_addr = false;
                }
                sdec_data(ctx, (uint8_t)ctx->buf[row][col]);
                ctx->prev[row][col] = ctx->buf[row][col];
            } else {
                need_addr = true;
            }
        }
    }
    return 0;
}

/* Force-write entire display (ignores diff) */
int sdec_flush_full(sdec_t *ctx)
{
    for (int row = 0; row < SDEC_ROWS; row++) {
        uint8_t addr = (row == 0) ? 0x80 : 0xC0;
        sdec_cmd(ctx, addr);
        for (int col = 0; col < SDEC_COLS; col++) {
            sdec_data(ctx, (uint8_t)ctx->buf[row][col]);
            ctx->prev[row][col] = ctx->buf[row][col];
        }
    }
    return 0;
}

/* ── Backlight ─────────────────────────────────────────────────────── */

int sdec_backlight(sdec_t *ctx, bool on)
{
    ctx->backlight = on;
    return 0;
}

/* ── Buttons ───────────────────────────────────────────────────────── */

int sdec_btn_read(sdec_t *ctx, uint8_t *btn)
{
    uint8_t raw = sdec_status(ctx);
    if (raw == ctx->last_btn) {
        *btn = 0;
        return 0;
    }
    ctx->last_btn = raw;
    *btn = raw;
    return 0;
}

const char *sdec_btn_name(uint8_t btn)
{
    switch (btn) {
    case 0x00: return "none";
    case 0x58: return "Left";
    case 0x60: return "Right";
    case 0x68: return "Up";
    case 0x70: return "Down";
    case 0xF8: return "Enter";
    default:   return "Unknown";
    }
}
