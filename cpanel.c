/*
 * cpanel.c — Check Point P-210/12200 LCD Panel Driver
 */

#include "cpanel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>

/* ── Serial helpers ────────────────────────────────────────────────── */

static int lcd_write(cpanel_t *ctx, const void *buf, size_t len)
{
    const uint8_t *p = buf;
    size_t written = 0;
    while (written < len) {
        ssize_t n = write(ctx->fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += (size_t)n;
    }
    tcdrain(ctx->fd);
    return 0;
}

static int lcd_byte(cpanel_t *ctx, uint8_t b)
{
    return lcd_write(ctx, &b, 1);
}

static int lcd_cmd2(cpanel_t *ctx, uint8_t a, uint8_t b)
{
    uint8_t buf[2] = {a, b};
    return lcd_write(ctx, buf, 2);
}

/* ── Core ──────────────────────────────────────────────────────────── */

int cpanel_open(cpanel_t *ctx, const char *device)
{
    memset(ctx, 0, sizeof(*ctx));

    if (!device) device = CPANEL_DEFAULT_DEV;
    snprintf(ctx->dev, sizeof(ctx->dev), "%s", device);

    ctx->fd = open(ctx->dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (ctx->fd < 0) {
        fprintf(stderr, "cpanel: open %s: %s\n", ctx->dev, strerror(errno));
        return -1;
    }

    struct termios tio;
    if (tcgetattr(ctx->fd, &tio) < 0) {
        fprintf(stderr, "cpanel: tcgetattr %s: %s\n", ctx->dev, strerror(errno));
        close(ctx->fd);
        ctx->fd = -1;
        return -1;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 5;
    tcsetattr(ctx->fd, TCSANOW, &tio);
    tcflush(ctx->fd, TCIOFLUSH);

    int flags = fcntl(ctx->fd, F_GETFL, 0);
    fcntl(ctx->fd, F_SETFL, flags & ~O_NONBLOCK);

    return 0;
}

void cpanel_close(cpanel_t *ctx)
{
    if (ctx->fd >= 0) { close(ctx->fd); ctx->fd = -1; }
}

int cpanel_init(cpanel_t *ctx)
{
    lcd_cmd2(ctx, CPANEL_ESC, CPANEL_INIT);
    usleep(100000);
    lcd_byte(ctx, CPANEL_CLEAR);
    usleep(100000);
    lcd_byte(ctx, CPANEL_HOME);
    usleep(50000);
    memset(ctx->buf, ' ', sizeof(ctx->buf));
    memset(ctx->prev, 0, sizeof(ctx->prev));
    for (int r = 0; r < CPANEL_ROWS; r++) {
        ctx->buf[r][CPANEL_COLS] = '\0';
        ctx->prev[r][CPANEL_COLS] = '\0';
    }
    ctx->dirty = 1;
    return 0;
}

int cpanel_clear(cpanel_t *ctx)
{
    memset(ctx->buf, ' ', sizeof(ctx->buf));
    for (int r = 0; r < CPANEL_ROWS; r++)
        ctx->buf[r][CPANEL_COLS] = '\0';
    ctx->dirty = 1;
    return 0;
}

/* ── Text ──────────────────────────────────────────────────────────── */

int cpanel_puts(cpanel_t *ctx, int row, const char *text)
{
    if (row < 0 || row >= CPANEL_ROWS) return -1;
    memset(ctx->buf[row], ' ', CPANEL_COLS);
    int len = strlen(text);
    if (len > CPANEL_COLS) len = CPANEL_COLS;
    memcpy(ctx->buf[row], text, len);
    ctx->buf[row][CPANEL_COLS] = '\0';
    return 0;
}

int cpanel_printf(cpanel_t *ctx, int row, const char *fmt, ...)
{
    char tmp[CPANEL_COLS + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    return cpanel_puts(ctx, row, tmp);
}

int cpanel_flush(cpanel_t *ctx)
{
    /*
     * Overwrite in place: home cursor, then write all 8 rows.
     * No clear command — avoids flicker. Each row is exactly 16 chars
     * (padded by cpanel_puts) so we overwrite the entire screen.
     */
    int changed = ctx->dirty;
    if (!changed) {
        for (int r = 0; r < CPANEL_ROWS; r++) {
            if (memcmp(ctx->buf[r], ctx->prev[r], CPANEL_COLS) != 0) {
                changed = 1;
                break;
            }
        }
    }
    if (!changed) return 0;

    /* Full ESC @ reset only on first frame or page change (dirty=1).
     * Regular updates just home the cursor and overwrite in place. */
    if (ctx->dirty) {
        lcd_cmd2(ctx, CPANEL_ESC, CPANEL_INIT);
        usleep(100000);
        lcd_byte(ctx, CPANEL_CLEAR);
        usleep(50000);
    }
    lcd_byte(ctx, CPANEL_HOME);
    usleep(10000);

    /* Write all rows — NO newlines, rely on auto-wrap at 21 chars.
     * Using 0x0A causes panel-interleaving on the EZIO-G500. */
    for (int r = 0; r < CPANEL_ROWS; r++) {
        lcd_write(ctx, ctx->buf[r], CPANEL_COLS);
        usleep(2000);
    }

    memcpy(ctx->prev, ctx->buf, sizeof(ctx->prev));
    ctx->dirty = 0;
    return 0;
}

/* ── Buttons ───────────────────────────────────────────────────────── */

/*
 * Poll for a button press. Returns button code (CPANEL_BTN_*) or 0 if
 * no button pressed within timeout_ms. Buttons auto-repeat while held.
 */
int cpanel_btn_poll(cpanel_t *ctx, int timeout_ms)
{
    struct pollfd pfd = { .fd = ctx->fd, .events = POLLIN };
    int elapsed = 0;
    while (elapsed < timeout_ms) {
        int r = poll(&pfd, 1, 50);
        if (r > 0 && (pfd.revents & POLLIN)) {
            uint8_t b;
            if (read(ctx->fd, &b, 1) == 1 && b >= 0x41 && b <= 0x47)
                return b;
        }
        elapsed += 50;
    }
    return 0;
}

const char *cpanel_btn_name(uint8_t btn)
{
    switch (btn) {
    case CPANEL_BTN_HELP:  return "?";
    case CPANEL_BTN_LEFT:  return "LEFT";
    case CPANEL_BTN_ESC:   return "ESC";
    case CPANEL_BTN_UP:    return "UP";
    case CPANEL_BTN_ENTER: return "ENTER";
    case CPANEL_BTN_DOWN:  return "DOWN";
    case CPANEL_BTN_RIGHT: return "RIGHT";
    default:               return NULL;
    }
}
