/*
 * ezio.c — EZIO-G500 LCD driver implementation
 *
 * Protocol from reverse engineering by Saint-Frater and tchatzi/EZIO-G500.
 * Serial: 115200 8N1, no flow control.
 * Display: 128x64 monochrome, dual-panel layout (left 64px, right 64px).
 * Each byte in graphics mode = 8 vertical pixels.
 */

#include "ezio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <math.h>

/* ── Built-in 5x8 font (uppercase + digits + punctuation) ──────────── */

/* Each glyph: 5 bytes, each byte is a column, LSB = top row */
static const uint8_t font_5x8[][5] = {
    /* ' ' */ {0x00, 0x00, 0x00, 0x00, 0x00},
    /* '!' */ {0x00, 0x00, 0x5F, 0x00, 0x00},
    /* '"' */ {0x00, 0x07, 0x00, 0x07, 0x00},
    /* '#' */ {0x14, 0x7F, 0x14, 0x7F, 0x14},
    /* '$' */ {0x24, 0x2A, 0x7F, 0x2A, 0x12},
    /* '%' */ {0x23, 0x13, 0x08, 0x64, 0x62},
    /* '&' */ {0x36, 0x49, 0x55, 0x22, 0x50},
    /* ''' */ {0x00, 0x05, 0x03, 0x00, 0x00},
    /* '(' */ {0x00, 0x1C, 0x22, 0x41, 0x00},
    /* ')' */ {0x00, 0x41, 0x22, 0x1C, 0x00},
    /* '*' */ {0x14, 0x08, 0x3E, 0x08, 0x14},
    /* '+' */ {0x08, 0x08, 0x3E, 0x08, 0x08},
    /* ',' */ {0x00, 0x50, 0x30, 0x00, 0x00},
    /* '-' */ {0x08, 0x08, 0x08, 0x08, 0x08},
    /* '.' */ {0x00, 0x60, 0x60, 0x00, 0x00},
    /* '/' */ {0x20, 0x10, 0x08, 0x04, 0x02},
    /* '0' */ {0x3E, 0x51, 0x49, 0x45, 0x3E},
    /* '1' */ {0x00, 0x42, 0x7F, 0x40, 0x00},
    /* '2' */ {0x42, 0x61, 0x51, 0x49, 0x46},
    /* '3' */ {0x21, 0x41, 0x45, 0x4B, 0x31},
    /* '4' */ {0x18, 0x14, 0x12, 0x7F, 0x10},
    /* '5' */ {0x27, 0x45, 0x45, 0x45, 0x39},
    /* '6' */ {0x3C, 0x4A, 0x49, 0x49, 0x30},
    /* '7' */ {0x01, 0x71, 0x09, 0x05, 0x03},
    /* '8' */ {0x36, 0x49, 0x49, 0x49, 0x36},
    /* '9' */ {0x06, 0x49, 0x49, 0x29, 0x1E},
    /* ':' */ {0x00, 0x36, 0x36, 0x00, 0x00},
    /* ';' */ {0x00, 0x56, 0x36, 0x00, 0x00},
    /* '<' */ {0x08, 0x14, 0x22, 0x41, 0x00},
    /* '=' */ {0x14, 0x14, 0x14, 0x14, 0x14},
    /* '>' */ {0x00, 0x41, 0x22, 0x14, 0x08},
    /* '?' */ {0x02, 0x01, 0x51, 0x09, 0x06},
    /* '@' */ {0x32, 0x49, 0x79, 0x41, 0x3E},
    /* 'A' */ {0x7E, 0x11, 0x11, 0x11, 0x7E},
    /* 'B' */ {0x7F, 0x49, 0x49, 0x49, 0x36},
    /* 'C' */ {0x3E, 0x41, 0x41, 0x41, 0x22},
    /* 'D' */ {0x7F, 0x41, 0x41, 0x22, 0x1C},
    /* 'E' */ {0x7F, 0x49, 0x49, 0x49, 0x41},
    /* 'F' */ {0x7F, 0x09, 0x09, 0x09, 0x01},
    /* 'G' */ {0x3E, 0x41, 0x49, 0x49, 0x7A},
    /* 'H' */ {0x7F, 0x08, 0x08, 0x08, 0x7F},
    /* 'I' */ {0x00, 0x41, 0x7F, 0x41, 0x00},
    /* 'J' */ {0x20, 0x40, 0x41, 0x3F, 0x01},
    /* 'K' */ {0x7F, 0x08, 0x14, 0x22, 0x41},
    /* 'L' */ {0x7F, 0x40, 0x40, 0x40, 0x40},
    /* 'M' */ {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    /* 'N' */ {0x7F, 0x04, 0x08, 0x10, 0x7F},
    /* 'O' */ {0x3E, 0x41, 0x41, 0x41, 0x3E},
    /* 'P' */ {0x7F, 0x09, 0x09, 0x09, 0x06},
    /* 'Q' */ {0x3E, 0x41, 0x51, 0x21, 0x5E},
    /* 'R' */ {0x7F, 0x09, 0x19, 0x29, 0x46},
    /* 'S' */ {0x46, 0x49, 0x49, 0x49, 0x31},
    /* 'T' */ {0x01, 0x01, 0x7F, 0x01, 0x01},
    /* 'U' */ {0x3F, 0x40, 0x40, 0x40, 0x3F},
    /* 'V' */ {0x1F, 0x20, 0x40, 0x20, 0x1F},
    /* 'W' */ {0x3F, 0x40, 0x38, 0x40, 0x3F},
    /* 'X' */ {0x63, 0x14, 0x08, 0x14, 0x63},
    /* 'Y' */ {0x07, 0x08, 0x70, 0x08, 0x07},
    /* 'Z' */ {0x61, 0x51, 0x49, 0x45, 0x43},
    /* '[' */ {0x00, 0x7F, 0x41, 0x41, 0x00},
    /* '\' */ {0x02, 0x04, 0x08, 0x10, 0x20},
    /* ']' */ {0x00, 0x41, 0x41, 0x7F, 0x00},
    /* '^' */ {0x04, 0x02, 0x01, 0x02, 0x04},
    /* '_' */ {0x40, 0x40, 0x40, 0x40, 0x40},
    /* '`' */ {0x00, 0x01, 0x02, 0x04, 0x00},
    /* 'a' */ {0x20, 0x54, 0x54, 0x54, 0x78},
    /* 'b' */ {0x7F, 0x48, 0x44, 0x44, 0x38},
    /* 'c' */ {0x38, 0x44, 0x44, 0x44, 0x20},
    /* 'd' */ {0x38, 0x44, 0x44, 0x48, 0x7F},
    /* 'e' */ {0x38, 0x54, 0x54, 0x54, 0x18},
    /* 'f' */ {0x08, 0x7E, 0x09, 0x01, 0x02},
    /* 'g' */ {0x0C, 0x52, 0x52, 0x52, 0x3E},
    /* 'h' */ {0x7F, 0x08, 0x04, 0x04, 0x78},
    /* 'i' */ {0x00, 0x44, 0x7D, 0x40, 0x00},
    /* 'j' */ {0x20, 0x40, 0x44, 0x3D, 0x00},
    /* 'k' */ {0x7F, 0x10, 0x28, 0x44, 0x00},
    /* 'l' */ {0x00, 0x41, 0x7F, 0x40, 0x00},
    /* 'm' */ {0x7C, 0x04, 0x18, 0x04, 0x78},
    /* 'n' */ {0x7C, 0x08, 0x04, 0x04, 0x78},
    /* 'o' */ {0x38, 0x44, 0x44, 0x44, 0x38},
    /* 'p' */ {0x7C, 0x14, 0x14, 0x14, 0x08},
    /* 'q' */ {0x08, 0x14, 0x14, 0x18, 0x7C},
    /* 'r' */ {0x7C, 0x08, 0x04, 0x04, 0x08},
    /* 's' */ {0x48, 0x54, 0x54, 0x54, 0x20},
    /* 't' */ {0x04, 0x3F, 0x44, 0x40, 0x20},
    /* 'u' */ {0x3C, 0x40, 0x40, 0x20, 0x7C},
    /* 'v' */ {0x1C, 0x20, 0x40, 0x20, 0x1C},
    /* 'w' */ {0x3C, 0x40, 0x30, 0x40, 0x3C},
    /* 'x' */ {0x44, 0x28, 0x10, 0x28, 0x44},
    /* 'y' */ {0x0C, 0x50, 0x50, 0x50, 0x3C},
    /* 'z' */ {0x44, 0x64, 0x54, 0x4C, 0x44},
};

static const uint8_t *font_glyph(char c)
{
    /* Font table starts at ' ' (0x20) through 'z' (0x7A) */
    if (c >= ' ' && c <= 'z')
        return font_5x8[c - ' '];
    return font_5x8[0]; /* space for unknown */
}

/* ── Serial helpers ────────────────────────────────────────────────── */

static int serial_write(ezio_t *ctx, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(ctx->fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("ezio: write");
            return -1;
        }
        written += n;
    }
    /* small delay to let the LCD process */
    usleep(1000);
    return 0;
}

static int serial_write_byte(ezio_t *ctx, uint8_t b)
{
    return serial_write(ctx, &b, 1);
}

static int serial_write2(ezio_t *ctx, uint8_t a, uint8_t b)
{
    uint8_t buf[2] = {a, b};
    return serial_write(ctx, buf, 2);
}

/* ── Core API ──────────────────────────────────────────────────────── */

int ezio_open(ezio_t *ctx, const char *device)
{
    memset(ctx, 0, sizeof(*ctx));

    if (!device)
        device = EZIO_DEFAULT_DEV;
    snprintf(ctx->dev, sizeof(ctx->dev), "%s", device);

    ctx->fd = open(ctx->dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ctx->fd < 0) {
        fprintf(stderr, "ezio: cannot open %s: %s\n", ctx->dev, strerror(errno));
        return -1;
    }

    /* Configure serial: 115200 8N1, no flow control, local mode */
    struct termios tio;
    if (tcgetattr(ctx->fd, &tio) < 0) {
        perror("ezio: tcgetattr");
        close(ctx->fd);
        return -1;
    }

    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);

    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cflag &= ~CSIZE;
    tio.c_cflag |= CS8;

    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 5;   /* 500ms read timeout */

    if (tcsetattr(ctx->fd, TCSANOW, &tio) < 0) {
        perror("ezio: tcsetattr");
        close(ctx->fd);
        return -1;
    }

    /* Flush any pending data */
    tcflush(ctx->fd, TCIOFLUSH);

    /* Clear O_NONBLOCK after setup */
    int flags = fcntl(ctx->fd, F_GETFL, 0);
    fcntl(ctx->fd, F_SETFL, flags & ~O_NONBLOCK);

    return 0;
}

void ezio_close(ezio_t *ctx)
{
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
}

int ezio_init(ezio_t *ctx)
{
    /* ESC @ = system reset */
    if (serial_write2(ctx, EZIO_CMD_INIT_ESC, EZIO_CMD_INIT_AT) < 0)
        return -1;
    usleep(50000);  /* 50ms for LCD to reset */
    ctx->gfx_mode = false;
    return ezio_clear(ctx);
}

int ezio_clear(ezio_t *ctx)
{
    if (serial_write_byte(ctx, EZIO_CMD_CLEAR) < 0)
        return -1;
    if (serial_write_byte(ctx, EZIO_CMD_HOME) < 0)
        return -1;
    usleep(5000);
    return 0;
}

/* ── Framebuffer operations ────────────────────────────────────────── */

int ezio_fb_clear(ezio_t *ctx)
{
    memset(ctx->fb, 0, EZIO_FB_SIZE);
    return 0;
}

int ezio_fb_pixel(ezio_t *ctx, int x, int y, bool on)
{
    if (x < 0 || x >= EZIO_WIDTH || y < 0 || y >= EZIO_HEIGHT)
        return -1;

    /*
     * Framebuffer layout: each byte = 8 vertical pixels.
     * The display is split into left (cols 0-63) and right (cols 64-127) panels.
     * Within each panel: byte index = col + (row/8) * 64
     * Bit within byte: row % 8, LSB = top
     */
    int panel_col = x % 64;
    int panel = x / 64;
    int page = y / 8;
    int bit = y % 8;
    int idx = panel * 512 + page * 64 + panel_col;

    if (on)
        ctx->fb[idx] |= (1 << bit);
    else
        ctx->fb[idx] &= ~(1 << bit);

    return 0;
}

int ezio_fb_line(ezio_t *ctx, int x0, int y0, int x1, int y1)
{
    /* Bresenham's line algorithm */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        ezio_fb_pixel(ctx, x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return 0;
}

int ezio_fb_rect(ezio_t *ctx, int x0, int y0, int x1, int y1, bool fill)
{
    if (fill) {
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++)
                ezio_fb_pixel(ctx, x, y, true);
    } else {
        ezio_fb_line(ctx, x0, y0, x1, y0);
        ezio_fb_line(ctx, x1, y0, x1, y1);
        ezio_fb_line(ctx, x1, y1, x0, y1);
        ezio_fb_line(ctx, x0, y1, x0, y0);
    }
    return 0;
}

int ezio_fb_bmp(ezio_t *ctx, const char *bmp_path)
{
    FILE *f = fopen(bmp_path, "rb");
    if (!f) {
        fprintf(stderr, "ezio: cannot open %s: %s\n", bmp_path, strerror(errno));
        return -1;
    }

    /* Read BMP header */
    uint8_t hdr[62];
    if (fread(hdr, 1, 62, f) < 62) {
        fprintf(stderr, "ezio: %s: too short for BMP header\n", bmp_path);
        fclose(f);
        return -1;
    }

    /* Verify BMP magic */
    if (hdr[0] != 'B' || hdr[1] != 'M') {
        fprintf(stderr, "ezio: %s: not a BMP file\n", bmp_path);
        fclose(f);
        return -1;
    }

    /* Get pixel data offset from header (cast to avoid UB on signed shift) */
    uint32_t px_offset = (uint32_t)hdr[10] | ((uint32_t)hdr[11] << 8) |
                         ((uint32_t)hdr[12] << 16) | ((uint32_t)hdr[13] << 24);
    int32_t bmp_w = (int32_t)((uint32_t)hdr[18] | ((uint32_t)hdr[19] << 8) |
                              ((uint32_t)hdr[20] << 16) | ((uint32_t)hdr[21] << 24));
    int32_t bmp_h = (int32_t)((uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8) |
                              ((uint32_t)hdr[24] << 16) | ((uint32_t)hdr[25] << 24));
    uint16_t bpp = (uint16_t)(hdr[28] | (hdr[29] << 8));

    if (bmp_w != EZIO_WIDTH || abs(bmp_h) != EZIO_HEIGHT) {
        fprintf(stderr, "ezio: %s: expected %dx%d, got %dx%d\n",
                bmp_path, EZIO_WIDTH, EZIO_HEIGHT, bmp_w, abs(bmp_h));
        fclose(f);
        return -1;
    }

    if (bpp != 1) {
        fprintf(stderr, "ezio: %s: expected 1bpp monochrome, got %d bpp\n", bmp_path, bpp);
        fclose(f);
        return -1;
    }

    /* Seek to pixel data */
    fseek(f, px_offset, SEEK_SET);

    /* BMP row stride: padded to 4-byte boundary */
    int stride = ((bmp_w + 31) / 32) * 4;
    uint8_t row[32];
    if (stride > (int)sizeof(row)) {
        fprintf(stderr, "ezio: %s: stride %d exceeds buffer\n", bmp_path, stride);
        fclose(f);
        return -1;
    }
    bool bottom_up = (bmp_h > 0);

    ezio_fb_clear(ctx);

    for (int r = 0; r < EZIO_HEIGHT; r++) {
        if (fread(row, 1, stride, f) < (size_t)stride) break;
        int y = bottom_up ? (EZIO_HEIGHT - 1 - r) : r;
        for (int x = 0; x < EZIO_WIDTH; x++) {
            int byte_idx = x / 8;
            int bit_idx = 7 - (x % 8);
            /* BMP monochrome: 0 = black (ink), 1 = white (background) */
            bool pixel_on = !(row[byte_idx] & (1 << bit_idx));
            if (pixel_on)
                ezio_fb_pixel(ctx, x, y, true);
        }
    }

    fclose(f);
    return 0;
}

/*
 * Flush framebuffer to LCD via graphics mode.
 *
 * The EZIO-G500 display is physically two 64x64 panels side by side.
 * The protocol sends data in 128-byte pages (8 pages total for 64 rows).
 * Within each page: first 64 bytes = left panel, next 64 bytes = right panel.
 * But the serial order is interleaved: left page, right page, alternating.
 *
 * From the Perl reference: even chunks = left panel, odd chunks = right panel.
 */
int ezio_fb_flush(ezio_t *ctx)
{
    /* Enter graphics mode: ESC G */
    if (serial_write2(ctx, EZIO_CMD_INIT_ESC, EZIO_CMD_GFX_MODE) < 0)
        return -1;
    usleep(5000);

    /*
     * Send framebuffer data.
     * The fb layout is: [left_panel: 512 bytes][right_panel: 512 bytes]
     * Each panel: 8 pages of 64 bytes.
     * Serial order: left_page0, right_page0, left_page1, right_page1, ...
     */
    uint8_t sendbuf[EZIO_FB_SIZE];
    int si = 0;
    for (int page = 0; page < 8; page++) {
        /* Left panel page */
        memcpy(&sendbuf[si], &ctx->fb[page * 64], 64);
        si += 64;
        /* Right panel page */
        memcpy(&sendbuf[si], &ctx->fb[512 + page * 64], 64);
        si += 64;
    }

    /* XOR invert (display expects inverted: 0=on, 1=off) */
    for (int i = 0; i < EZIO_FB_SIZE; i++)
        sendbuf[i] ^= 0xFF;

    if (serial_write(ctx, sendbuf, EZIO_FB_SIZE) < 0)
        return -1;

    ctx->gfx_mode = true;
    usleep(50000); /* let LCD render */
    return 0;
}

/* ── Text rendering (into framebuffer) ─────────────────────────────── */

int ezio_text(ezio_t *ctx, int x, int y, const char *text)
{
    int cx = x;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            cx = x;
            y += 9; /* 8px font + 1px spacing */
            continue;
        }

        const uint8_t *glyph = font_glyph(*p);
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 8; row++) {
                if (glyph[col] & (1 << row))
                    ezio_fb_pixel(ctx, cx + col, y + row, true);
            }
        }
        cx += 6; /* 5px glyph + 1px spacing */

        if (cx + 5 > EZIO_WIDTH) {
            cx = x;
            y += 9;
        }
    }
    return 0;
}

int ezio_printf(ezio_t *ctx, int x, int y, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return ezio_text(ctx, x, y, buf);
}

/* ── LEDs ──────────────────────────────────────────────────────────── */

/*
 * LED protocol (from Saint-Frater reverse engineering):
 * Send: ESC, 0x4C, <led_mask>
 * LED mask encodes which LEDs and colors.
 * Each LED has 2 bits: bit0 = green, bit1 = red
 * LED1 = bits 0-1, LED2 = bits 2-3, LED3 = bits 4-5
 */
int ezio_led(ezio_t *ctx, int led, ezio_led_color_t color)
{
    if (led < 0 || led > 2)
        return -1;

    int shift = led * 2;
    ctx->led_state &= ~(0x03 << shift);
    ctx->led_state |= ((uint8_t)color & 0x03) << shift;

    uint8_t cmd[3] = { EZIO_CMD_INIT_ESC, 0x4C, ctx->led_state };
    return serial_write(ctx, cmd, 3);
}

/* ── Buttons ───────────────────────────────────────────────────────── */

int ezio_btn_read(ezio_t *ctx, uint8_t *buttons)
{
    /* Send button query */
    if (serial_write_byte(ctx, EZIO_CMD_BTN_READ) < 0)
        return -1;

    usleep(50000); /* wait for response */

    uint8_t resp;
    ssize_t n = read(ctx->fd, &resp, 1);
    if (n <= 0) {
        *buttons = 0;
        return (n == 0) ? 0 : -1;
    }

    *buttons = resp;
    return 0;
}

/* ── Backlight ─────────────────────────────────────────────────────── */

int ezio_backlight(ezio_t *ctx, bool on)
{
    /* ESC 0x42 for backlight on, ESC 0x46 for off (from EZIO-100 compat) */
    uint8_t cmd = on ? 0x42 : 0x46;
    return serial_write2(ctx, EZIO_CMD_INIT_ESC, cmd);
}
