/*
 * cpanel.h — Check Point P-210/12200 LCD Panel Driver
 *
 * EZIO-G500 128x64 graphical LCD in text mode over RS-232.
 * 16 columns x 8 rows, 115200 8N1, /dev/cuau1 (FreeBSD) or /dev/ttyS1 (Linux).
 *
 * Reverse-engineered from hardware by paschal + Claude, March 2026.
 */

#ifndef CPANEL_H
#define CPANEL_H

#include <stdint.h>
#include <stdbool.h>

#define CPANEL_COLS      16
#define CPANEL_ROWS       8
#define CPANEL_BAUD  115200

#ifdef __FreeBSD__
#define CPANEL_DEFAULT_DEV  "/dev/cuau1"
#else
#define CPANEL_DEFAULT_DEV  "/dev/ttyS1"
#endif

/* Protocol bytes */
#define CPANEL_ESC       0x1B
#define CPANEL_INIT      0x40   /* ESC @ = reset */
#define CPANEL_CLEAR     0x0C   /* form feed = clear screen */
#define CPANEL_HOME      0x0B   /* vertical tab = cursor home */
#define CPANEL_NEWLINE   0x0A   /* line feed = next row */
#define CPANEL_GFX_MODE  0x47   /* ESC G = enter graphics mode */

typedef struct {
    int   fd;
    char  dev[128];
    char  buf[CPANEL_ROWS][CPANEL_COLS + 1];
} cpanel_t;

/* Core */
int   cpanel_open(cpanel_t *ctx, const char *device);
void  cpanel_close(cpanel_t *ctx);
int   cpanel_init(cpanel_t *ctx);
int   cpanel_clear(cpanel_t *ctx);

/* Text */
int   cpanel_puts(cpanel_t *ctx, int row, const char *text);
int   cpanel_printf(cpanel_t *ctx, int row, const char *fmt, ...)
      __attribute__((format(printf, 3, 4)));
int   cpanel_flush(cpanel_t *ctx);

/* Buttons (read from serial) */
int   cpanel_read(cpanel_t *ctx, uint8_t *buf, int max, int timeout_ms);

#endif
