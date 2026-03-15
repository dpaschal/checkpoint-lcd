/*
 * ezio.h — EZIO-G500 LCD driver for Check Point P-210/12200 appliances
 *
 * Portwell EZIO-G500: 128x64 graphical LCD, RS-232, 7 buttons, 3 bi-color LEDs
 * Protocol reverse-engineered from Saint-Frater + tchatzi/EZIO-G500
 */

#ifndef EZIO_H
#define EZIO_H

#include <stdint.h>
#include <stdbool.h>

/* Display geometry */
#define EZIO_WIDTH       128
#define EZIO_HEIGHT       64
#define EZIO_FB_SIZE    1024   /* 128 * 64 / 8 = 1024 bytes */

/* Serial defaults */
#define EZIO_DEFAULT_DEV_FREEBSD  "/dev/cuau1"
#define EZIO_DEFAULT_DEV_LINUX    "/dev/ttyS1"
#define EZIO_BAUD                 115200

#ifdef __FreeBSD__
#define EZIO_DEFAULT_DEV  EZIO_DEFAULT_DEV_FREEBSD
#else
#define EZIO_DEFAULT_DEV  EZIO_DEFAULT_DEV_LINUX
#endif

/* Protocol commands */
#define EZIO_CMD_INIT_ESC    0x1B
#define EZIO_CMD_INIT_AT     0x40   /* ESC @ = system reset */
#define EZIO_CMD_CLEAR       0x0C   /* clear display */
#define EZIO_CMD_HOME        0x0B   /* cursor to origin */
#define EZIO_CMD_GFX_MODE    0x47   /* ESC G = enter graphics mode */
#define EZIO_CMD_CLR_SCREEN  0x38   /* alternate clear */
#define EZIO_CMD_CURSOR_LEFT 0x28   /* cursor leftmost */
#define EZIO_CMD_BTN_READ    0x75   /* read button state */

/* LED indices */
#define EZIO_LED_1  0
#define EZIO_LED_2  1
#define EZIO_LED_3  2

/* LED colors (bi-color: red + green = amber) */
typedef enum {
    EZIO_LED_OFF    = 0,
    EZIO_LED_GREEN  = 1,
    EZIO_LED_RED    = 2,
    EZIO_LED_AMBER  = 3,  /* both on */
} ezio_led_color_t;

/* Button bits (from serial read) */
typedef enum {
    EZIO_BTN_UP     = 0x01,
    EZIO_BTN_DOWN   = 0x02,
    EZIO_BTN_LEFT   = 0x04,
    EZIO_BTN_RIGHT  = 0x08,
    EZIO_BTN_ENTER  = 0x10,
    EZIO_BTN_ESC    = 0x20,
    EZIO_BTN_MENU   = 0x40,
} ezio_btn_t;

/* Device context */
typedef struct {
    int           fd;
    char          dev[128];
    uint8_t       fb[EZIO_FB_SIZE];   /* local framebuffer */
    bool          gfx_mode;
} ezio_t;

/* Core API */
int   ezio_open(ezio_t *ctx, const char *device);
void  ezio_close(ezio_t *ctx);
int   ezio_init(ezio_t *ctx);
int   ezio_clear(ezio_t *ctx);

/* Text (uses built-in 5x8 font, renders to framebuffer) */
int   ezio_text(ezio_t *ctx, int x, int y, const char *text);
int   ezio_printf(ezio_t *ctx, int x, int y, const char *fmt, ...)
      __attribute__((format(printf, 4, 5)));

/* Graphics */
int   ezio_fb_clear(ezio_t *ctx);
int   ezio_fb_pixel(ezio_t *ctx, int x, int y, bool on);
int   ezio_fb_line(ezio_t *ctx, int x0, int y0, int x1, int y1);
int   ezio_fb_rect(ezio_t *ctx, int x0, int y0, int x1, int y1, bool fill);
int   ezio_fb_bmp(ezio_t *ctx, const char *bmp_path);
int   ezio_fb_flush(ezio_t *ctx);   /* send framebuffer to LCD */

/* LEDs */
int   ezio_led(ezio_t *ctx, int led, ezio_led_color_t color);

/* Buttons */
int   ezio_btn_read(ezio_t *ctx, uint8_t *buttons);

/* Backlight */
int   ezio_backlight(ezio_t *ctx, bool on);

#endif /* EZIO_H */
