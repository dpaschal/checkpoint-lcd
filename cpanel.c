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

static int lcd_write(cpanel_t *ctx, const void *buf, int len)
{
    const uint8_t *p = buf;
    int written = 0;
    while (written < len) {
        int n = write(ctx->fd, p + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        written += n;
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
    tcgetattr(ctx->fd, &tio);
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
    for (int r = 0; r < CPANEL_ROWS; r++)
        ctx->buf[r][CPANEL_COLS] = '\0';
    return 0;
}

int cpanel_clear(cpanel_t *ctx)
{
    lcd_byte(ctx, CPANEL_CLEAR);
    usleep(100000);
    lcd_byte(ctx, CPANEL_HOME);
    usleep(50000);
    memset(ctx->buf, ' ', sizeof(ctx->buf));
    for (int r = 0; r < CPANEL_ROWS; r++)
        ctx->buf[r][CPANEL_COLS] = '\0';
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
    lcd_byte(ctx, CPANEL_CLEAR);
    usleep(50000);
    lcd_byte(ctx, CPANEL_HOME);
    usleep(20000);

    for (int r = 0; r < CPANEL_ROWS; r++) {
        lcd_write(ctx, ctx->buf[r], CPANEL_COLS);
        if (r < CPANEL_ROWS - 1)
            lcd_byte(ctx, CPANEL_NEWLINE);
        usleep(5000);
    }
    return 0;
}

/* ── Read ──────────────────────────────────────────────────────────── */

int cpanel_read(cpanel_t *ctx, uint8_t *buf, int max, int timeout_ms)
{
    struct pollfd pfd = { .fd = ctx->fd, .events = POLLIN };
    int total = 0, idle = 0;
    while (idle < timeout_ms && total < max) {
        int r = poll(&pfd, 1, 50);
        if (r > 0 && (pfd.revents & POLLIN)) {
            int n = read(ctx->fd, buf + total, max - total);
            if (n > 0) { total += n; idle = 0; }
            else idle += 50;
        } else idle += 50;
    }
    return total;
}
