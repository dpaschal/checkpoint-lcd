/*
 * sdeclcd.h — SDEC LCD driver for Check Point P-210/12200 appliances
 *
 * Reverse-engineered from OPNsense's sdeclcd.so (v0.5.9) by disassembly.
 *
 * Hardware: HD44780-compatible 20x2 character LCD behind LPT1 parallel port.
 * Interface: Direct I/O port access (0x378/0x379/0x37a).
 * Buttons: 5 buttons readable from LPT1 status port.
 * Backlight: Controlled via strobe pattern on control port.
 *
 * Requires root / access to /dev/io (FreeBSD) or ioperm() (Linux).
 */

#ifndef SDECLCD_H
#define SDECLCD_H

#include <stdint.h>
#include <stdbool.h>

/* Display geometry (from disassembly: width=0x14, height=0x02, cell=5x8) */
#define SDEC_COLS        20
#define SDEC_ROWS         2
#define SDEC_CELL_W       5
#define SDEC_CELL_H       8
#define SDEC_BUF_SIZE    (SDEC_COLS * SDEC_ROWS)  /* 40 chars */

/* I/O ports — LPT1 (from disassembly of sdeclcd.so) */
#define SDEC_PORT_DATA    0x378   /* OUT: data byte to LCD */
#define SDEC_PORT_STATUS  0x379   /* IN:  button state (upper 5 bits) */
#define SDEC_PORT_CTRL    0x37a   /* OUT: strobe/control signals */

/* Strobe patterns (from disassembly XOR analysis) */
#define SDEC_STROBE_CMD   0x09   /* XOR toggle for command strobe */
#define SDEC_STROBE_DATA  0x0B   /* XOR toggle for data strobe */
#define SDEC_STROBE_ALT   0x03   /* alternate strobe pattern */

/* HD44780 command bytes sent via data port */
#define SDEC_HD_CLEAR     0x01   /* clear display */
#define SDEC_HD_HOME      0x02   /* cursor home */
#define SDEC_HD_ENTRY     0x06   /* entry mode: increment, no shift */
#define SDEC_HD_DISP_ON   0x0C   /* display on, cursor off, blink off */
#define SDEC_HD_FUNC_SET  0x38   /* 8-bit, 2 lines, 5x8 font */
#define SDEC_HD_LINE1     0x80   /* DDRAM address: line 1 start */
#define SDEC_HD_LINE2     0xC0   /* DDRAM address: line 2 start */

/* Button masks from status port (0x379), upper 5 bits, masked with 0xF8 */
#define SDEC_BTN_MASK     0xF8
/* Button values (from disassembly key mapping table) */
#define SDEC_BTN_NONE     0x00
#define SDEC_BTN_LEFT     0x58   /* "Left" key string in binary */
#define SDEC_BTN_RIGHT    0x60   /* "Right" */
#define SDEC_BTN_UP       0x68   /* "Up" */
#define SDEC_BTN_DOWN     0x70   /* "Down" */
#define SDEC_BTN_ENTER    0x78   /* derived from jump table spacing */

/* Device context */
typedef struct {
    int           io_fd;        /* /dev/io fd (FreeBSD) or -1 (Linux ioperm) */
    bool          opened;
    bool          backlight;
    char          buf[2][SDEC_COLS + 1];  /* display buffer (2 lines, null-term) */
    char          prev[2][SDEC_COLS + 1]; /* previous state for diff flush */
    uint8_t       last_btn;     /* last button reading */
} sdec_t;

/* Core API */
int   sdec_open(sdec_t *ctx);
void  sdec_close(sdec_t *ctx);
int   sdec_init(sdec_t *ctx);

/* Display */
int   sdec_clear(sdec_t *ctx);
int   sdec_puts(sdec_t *ctx, int row, const char *text);
int   sdec_printf(sdec_t *ctx, int row, const char *fmt, ...)
      __attribute__((format(printf, 3, 4)));
int   sdec_putc(sdec_t *ctx, int row, int col, char c);
int   sdec_flush(sdec_t *ctx);       /* send changed chars to LCD */
int   sdec_flush_full(sdec_t *ctx);  /* force-write entire display */

/* Backlight */
int   sdec_backlight(sdec_t *ctx, bool on);

/* Buttons */
int   sdec_btn_read(sdec_t *ctx, uint8_t *btn);
const char *sdec_btn_name(uint8_t btn);

/* Low-level (for debugging) */
void  sdec_cmd(sdec_t *ctx, uint8_t cmd);
void  sdec_data(sdec_t *ctx, uint8_t data);
uint8_t sdec_status(sdec_t *ctx);

#endif /* SDECLCD_H */
