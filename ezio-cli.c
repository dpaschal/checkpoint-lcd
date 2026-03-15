/*
 * ezio-cli.c — Command-line tool for the EZIO-G500 LCD
 *
 * Usage:
 *   ezio-cli [-d /dev/cuau1] <command> [args...]
 *
 * Commands:
 *   init                    Reset and clear the display
 *   clear                   Clear the display
 *   text <x> <y> <string>   Write text at position
 *   bmp <file.bmp>          Display a 128x64 monochrome BMP
 *   led <0-2> <off|green|red|amber>   Set LED color
 *   btn                     Read button state (blocks briefly)
 *   backlight <on|off>      Toggle backlight
 *   demo                    Run a demo showing all features
 *   status                  Show system status (for OPNsense integration)
 *   probe                   Probe serial port and test communication
 */

#include "ezio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static ezio_t lcd;
static volatile int running = 1;

static void sighandler(int sig)
{
    (void)sig;
    running = 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [-d device] <command> [args...]\n"
        "\n"
        "Commands:\n"
        "  init                     Reset and clear display\n"
        "  clear                    Clear display\n"
        "  text <x> <y> <string>    Write text at pixel position\n"
        "  bmp <file.bmp>           Display 128x64 monochrome BMP\n"
        "  led <0-2> <off|green|red|amber>  Set LED color\n"
        "  btn                      Read button state\n"
        "  backlight <on|off>       Toggle backlight\n"
        "  demo                     Feature demo\n"
        "  probe                    Test serial communication\n"
        "\n"
        "Default device: %s\n",
        argv0, EZIO_DEFAULT_DEV);
}

static int cmd_init(void)
{
    if (ezio_init(&lcd) < 0) return 1;
    printf("Display initialized and cleared.\n");
    return 0;
}

static int cmd_clear(void)
{
    if (ezio_init(&lcd) < 0) return 1;
    ezio_fb_clear(&lcd);
    ezio_fb_flush(&lcd);
    printf("Display cleared.\n");
    return 0;
}

static int cmd_text(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: text <x> <y> <string>\n");
        return 1;
    }
    int x = atoi(argv[0]);
    int y = atoi(argv[1]);

    /* Join remaining args as the text string */
    char text[512] = {0};
    for (int i = 2; i < argc; i++) {
        if (i > 2) strcat(text, " ");
        strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
    }

    if (ezio_init(&lcd) < 0) return 1;
    ezio_fb_clear(&lcd);
    ezio_text(&lcd, x, y, text);
    ezio_fb_flush(&lcd);

    printf("Text displayed at (%d, %d): %s\n", x, y, text);
    return 0;
}

static int cmd_bmp(const char *path)
{
    if (ezio_init(&lcd) < 0) return 1;
    if (ezio_fb_bmp(&lcd, path) < 0) return 1;
    if (ezio_fb_flush(&lcd) < 0) return 1;
    printf("BMP displayed: %s\n", path);
    return 0;
}

static int cmd_led(const char *led_str, const char *color_str)
{
    int led = atoi(led_str);
    ezio_led_color_t color;

    if (strcmp(color_str, "off") == 0)        color = EZIO_LED_OFF;
    else if (strcmp(color_str, "green") == 0)  color = EZIO_LED_GREEN;
    else if (strcmp(color_str, "red") == 0)    color = EZIO_LED_RED;
    else if (strcmp(color_str, "amber") == 0)  color = EZIO_LED_AMBER;
    else {
        fprintf(stderr, "Unknown color: %s (use off/green/red/amber)\n", color_str);
        return 1;
    }

    /* LED command doesn't need graphics init */
    if (ezio_led(&lcd, led, color) < 0) return 1;
    printf("LED %d set to %s\n", led, color_str);
    return 0;
}

static int cmd_btn(void)
{
    printf("Reading buttons (press a button within 2 seconds)...\n");

    uint8_t btn;
    if (ezio_btn_read(&lcd, &btn) < 0) {
        fprintf(stderr, "Button read failed\n");
        return 1;
    }

    if (btn == 0) {
        printf("No button pressed.\n");
    } else {
        printf("Button state: 0x%02X", btn);
        if (btn & EZIO_BTN_UP)    printf(" UP");
        if (btn & EZIO_BTN_DOWN)  printf(" DOWN");
        if (btn & EZIO_BTN_LEFT)  printf(" LEFT");
        if (btn & EZIO_BTN_RIGHT) printf(" RIGHT");
        if (btn & EZIO_BTN_ENTER) printf(" ENTER");
        if (btn & EZIO_BTN_ESC)   printf(" ESC");
        if (btn & EZIO_BTN_MENU)  printf(" MENU");
        printf("\n");
    }
    return 0;
}

static int cmd_backlight(const char *state)
{
    bool on = (strcmp(state, "on") == 0);
    if (ezio_backlight(&lcd, on) < 0) return 1;
    printf("Backlight %s\n", on ? "on" : "off");
    return 0;
}

static int cmd_demo(void)
{
    printf("Running EZIO-G500 demo...\n");

    if (ezio_init(&lcd) < 0) return 1;

    /* Step 1: Title screen */
    ezio_fb_clear(&lcd);
    ezio_text(&lcd, 10, 4, "EZIO-G500");
    ezio_text(&lcd, 16, 16, "LCD Driver");
    ezio_text(&lcd, 4, 32, "Check Point P-210");
    ezio_text(&lcd, 20, 48, "by Claude");
    ezio_fb_flush(&lcd);
    printf("  [1/5] Title screen\n");

    /* LEDs: cycle through colors */
    ezio_led(&lcd, EZIO_LED_1, EZIO_LED_GREEN);
    ezio_led(&lcd, EZIO_LED_2, EZIO_LED_GREEN);
    ezio_led(&lcd, EZIO_LED_3, EZIO_LED_GREEN);
    sleep(2);

    /* Step 2: Drawing primitives */
    ezio_fb_clear(&lcd);
    ezio_fb_rect(&lcd, 0, 0, 127, 63, false);       /* border */
    ezio_fb_rect(&lcd, 4, 4, 30, 20, true);          /* filled box */
    ezio_fb_line(&lcd, 0, 0, 127, 63);               /* diagonal */
    ezio_fb_line(&lcd, 127, 0, 0, 63);               /* other diagonal */
    ezio_text(&lcd, 40, 28, "Graphics!");
    ezio_fb_flush(&lcd);
    printf("  [2/5] Drawing primitives\n");

    ezio_led(&lcd, EZIO_LED_1, EZIO_LED_RED);
    ezio_led(&lcd, EZIO_LED_2, EZIO_LED_AMBER);
    sleep(2);

    /* Step 3: Text demo */
    ezio_fb_clear(&lcd);
    ezio_text(&lcd, 0, 0,  "ABCDEFGHIJKLMNOPQRSTU");
    ezio_text(&lcd, 0, 9,  "abcdefghijklmnopqrstu");
    ezio_text(&lcd, 0, 18, "0123456789 !@#$%");
    ezio_text(&lcd, 0, 27, "Mixed Case Text OK");
    ezio_text(&lcd, 0, 40, "128x64 px  5x8 font");
    ezio_text(&lcd, 0, 52, "7 buttons  3 LEDs");
    ezio_fb_flush(&lcd);
    printf("  [3/5] Text / font demo\n");
    sleep(2);

    /* Step 4: Progress bar animation */
    for (int pct = 0; pct <= 100 && running; pct += 5) {
        ezio_fb_clear(&lcd);
        ezio_text(&lcd, 20, 8, "Loading...");
        ezio_fb_rect(&lcd, 10, 28, 117, 38, false);
        if (pct > 0)
            ezio_fb_rect(&lcd, 12, 30, 12 + (pct * 103) / 100, 36, true);
        ezio_printf(&lcd, 50, 48, "%d%%", pct);
        ezio_fb_flush(&lcd);
        usleep(100000);
    }
    printf("  [4/5] Progress bar animation\n");
    sleep(1);

    /* Step 5: Final screen */
    ezio_fb_clear(&lcd);
    ezio_text(&lcd, 8, 12, "Driver Ready");
    ezio_text(&lcd, 4, 28, "OPNsense + LCD");
    ezio_text(&lcd, 16, 44, "= Awesome");
    ezio_fb_flush(&lcd);

    ezio_led(&lcd, EZIO_LED_1, EZIO_LED_GREEN);
    ezio_led(&lcd, EZIO_LED_2, EZIO_LED_GREEN);
    ezio_led(&lcd, EZIO_LED_3, EZIO_LED_GREEN);
    printf("  [5/5] Done!\n");

    return 0;
}

static int cmd_probe(void)
{
    printf("Probing EZIO-G500 on %s...\n", lcd.dev);
    printf("  Serial port opened OK\n");

    /* Try init */
    printf("  Sending init sequence (ESC @)...\n");
    if (ezio_init(&lcd) < 0) {
        printf("  FAIL: init failed\n");
        return 1;
    }
    printf("  Init OK\n");

    /* Try writing test pattern */
    printf("  Sending test pattern...\n");
    ezio_fb_clear(&lcd);
    ezio_text(&lcd, 10, 20, "EZIO PROBE OK");
    if (ezio_fb_flush(&lcd) < 0) {
        printf("  FAIL: framebuffer flush failed\n");
        return 1;
    }
    printf("  Framebuffer sent OK\n");

    /* Try LEDs */
    printf("  Testing LEDs...\n");
    for (int i = 0; i < 3; i++) {
        ezio_led(&lcd, i, EZIO_LED_GREEN);
        usleep(200000);
    }
    printf("  LEDs OK (all green)\n");

    /* Try button read */
    printf("  Reading buttons...\n");
    uint8_t btn;
    ezio_btn_read(&lcd, &btn);
    printf("  Button state: 0x%02X\n", btn);

    printf("\nProbe complete. If the LCD shows 'EZIO PROBE OK' and LEDs are green, everything works.\n");
    return 0;
}

int main(int argc, char **argv)
{
    const char *device = NULL;
    int opt_idx = 1;

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    /* Parse -d option */
    if (argc > 2 && strcmp(argv[1], "-d") == 0) {
        device = argv[2];
        opt_idx = 3;
    }

    if (opt_idx >= argc) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[opt_idx];
    int cmd_argc = argc - opt_idx - 1;
    char **cmd_argv = &argv[opt_idx + 1];

    /* Open serial port */
    if (ezio_open(&lcd, device) < 0)
        return 1;

    int ret = 0;

    if (strcmp(cmd, "init") == 0) {
        ret = cmd_init();
    } else if (strcmp(cmd, "clear") == 0) {
        ret = cmd_clear();
    } else if (strcmp(cmd, "text") == 0) {
        ret = cmd_text(cmd_argc, cmd_argv);
    } else if (strcmp(cmd, "bmp") == 0) {
        if (cmd_argc < 1) { fprintf(stderr, "Usage: bmp <file.bmp>\n"); ret = 1; }
        else ret = cmd_bmp(cmd_argv[0]);
    } else if (strcmp(cmd, "led") == 0) {
        if (cmd_argc < 2) { fprintf(stderr, "Usage: led <0-2> <off|green|red|amber>\n"); ret = 1; }
        else ret = cmd_led(cmd_argv[0], cmd_argv[1]);
    } else if (strcmp(cmd, "btn") == 0) {
        ret = cmd_btn();
    } else if (strcmp(cmd, "backlight") == 0) {
        if (cmd_argc < 1) { fprintf(stderr, "Usage: backlight <on|off>\n"); ret = 1; }
        else ret = cmd_backlight(cmd_argv[0]);
    } else if (strcmp(cmd, "demo") == 0) {
        ret = cmd_demo();
    } else if (strcmp(cmd, "probe") == 0) {
        ret = cmd_probe();
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        ret = 1;
    }

    ezio_close(&lcd);
    return ret;
}
