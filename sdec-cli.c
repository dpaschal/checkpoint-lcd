/*
 * sdec-cli.c — Command-line tool for the Check Point P-210/12200 LCD
 *
 * Usage:
 *   sdec-cli <command> [args...]
 *
 * Commands:
 *   init                     Reset and initialize display
 *   clear                    Clear the display
 *   text <row> <string>      Write text on row 0 or 1
 *   write <line1> [line2]    Write both lines at once
 *   btn                      Read button state (poll)
 *   backlight <on|off>       Toggle backlight
 *   demo                     Animated demo
 *   probe                    Test hardware communication
 *   monitor                  Live system status display
 *   raw-cmd <hex>            Send raw HD44780 command byte
 *   raw-data <hex>           Send raw data byte
 */

#include "sdeclcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static sdec_t lcd;
static volatile int running = 1;

static void sighandler(int sig) { (void)sig; running = 0; }

static void usage(const char *argv0)
{
    fprintf(stderr,
        "SDEC LCD Tool for Check Point P-210/12200\n"
        "Reverse-engineered parallel port HD44780 driver\n\n"
        "Usage: %s <command> [args...]\n\n"
        "Commands:\n"
        "  init                     Reset and init display\n"
        "  clear                    Clear display\n"
        "  text <0|1> <string>      Write text on a row\n"
        "  write <line1> [line2]    Write both lines\n"
        "  btn                      Poll buttons (Ctrl+C to stop)\n"
        "  backlight <on|off>       Toggle backlight\n"
        "  demo                     Animated feature demo\n"
        "  probe                    Test hardware comms\n"
        "  monitor                  Live system status\n"
        "  raw-cmd <hex-byte>       Send raw command\n"
        "  raw-data <hex-byte>      Send raw data\n\n"
        "Requires root (direct I/O port access).\n"
        "I/O ports: 0x378 (data), 0x379 (status/buttons), 0x37a (control)\n",
        argv0);
}

static int cmd_probe(void)
{
    printf("Probing SDEC LCD on LPT1 (0x378-0x37a)...\n");
    printf("  I/O port access: OK\n");

    printf("  Sending HD44780 init sequence...\n");
    if (sdec_init(&lcd) < 0) {
        printf("  FAIL: init failed\n");
        return 1;
    }
    printf("  Init OK (20x2 character display)\n");

    printf("  Writing test pattern (full flush)...\n");
    sdec_puts(&lcd, 0, "SDEC PROBE OK");
    sdec_puts(&lcd, 1, "Check Point P-210");
    sdec_flush_full(&lcd);
    printf("  Display written\n");

    printf("  Reading button status port...\n");
    uint8_t btn;
    sdec_btn_read(&lcd, &btn);
    printf("  Button raw: 0x%02X (%s)\n", lcd.last_btn, sdec_btn_name(lcd.last_btn));

    printf("\nProbe complete.\n");
    printf("If LCD shows 'SDEC PROBE OK' — we're in business.\n");
    return 0;
}

static int cmd_demo(void)
{
    printf("Running SDEC LCD demo...\n");

    sdec_init(&lcd);

    /* Step 1: Welcome */
    sdec_puts(&lcd, 0, "  Check Point P-210");
    sdec_puts(&lcd, 1, "  LCD Driver v1.0");
    sdec_flush(&lcd);
    printf("  [1/5] Title\n");
    sleep(2);

    /* Step 2: Credits */
    sdec_puts(&lcd, 0, "Reverse-engineered");
    sdec_puts(&lcd, 1, "by Claude + paschal");
    sdec_flush(&lcd);
    printf("  [2/5] Credits\n");
    sleep(2);

    /* Step 3: Scrolling text */
    const char *scroll = "  This driver was reverse-engineered from the OPNsense sdeclcd.so binary!  ";
    int slen = strlen(scroll);
    printf("  [3/5] Scrolling text\n");
    for (int i = 0; i < slen - 20 && running; i++) {
        char line[21];
        strncpy(line, scroll + i, 20);
        line[20] = '\0';
        sdec_puts(&lcd, 0, "== Scroll Demo ==");
        sdec_puts(&lcd, 1, line);
        sdec_flush(&lcd);
        usleep(150000);
    }

    /* Step 4: Counter */
    printf("  [4/5] Counter\n");
    for (int i = 0; i <= 100 && running; i += 5) {
        char bar[21] = {0};
        int filled = i * 16 / 100;
        bar[0] = '[';
        for (int j = 1; j <= 16; j++)
            bar[j] = (j <= filled) ? '#' : '-';
        bar[17] = ']';
        bar[18] = '\0';

        sdec_printf(&lcd, 0, "Progress: %3d%%", i);
        sdec_puts(&lcd, 1, bar);
        sdec_flush(&lcd);
        usleep(100000);
    }
    sleep(1);

    /* Step 5: Done */
    sdec_puts(&lcd, 0, "  Driver Ready!");
    sdec_puts(&lcd, 1, "OPNsense+LCD=Win");
    sdec_flush(&lcd);
    printf("  [5/5] Done!\n");

    return 0;
}

static int cmd_monitor(void)
{
    printf("Live system monitor (Ctrl+C to stop)...\n");

    sdec_init(&lcd);

    int page = 0;
    time_t last_switch = time(NULL);

    while (running) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        /* Auto-rotate pages every 5 seconds */
        if (now - last_switch >= 5) {
            page = (page + 1) % 3;
            last_switch = now;
        }

        /* Check buttons for manual page switch */
        uint8_t btn;
        sdec_btn_read(&lcd, &btn);
        if (btn == SDEC_BTN_RIGHT || btn == SDEC_BTN_DOWN) {
            page = (page + 1) % 3;
            last_switch = now;
        } else if (btn == SDEC_BTN_LEFT || btn == SDEC_BTN_UP) {
            page = (page + 2) % 3; /* -1 mod 3 */
            last_switch = now;
        }

        switch (page) {
        case 0:
            sdec_printf(&lcd, 0, "fw01.paschal %d/%d", page + 1, 3);
            sdec_printf(&lcd, 1, "%02d:%02d:%02d  OPNsense",
                        tm->tm_hour, tm->tm_min, tm->tm_sec);
            break;
        case 1:
            sdec_puts(&lcd, 0, "LAN:192.168.1.1");
            sdec_puts(&lcd, 1, "WAN: not connected");
            break;
        case 2:
            sdec_printf(&lcd, 0, "Uptime: %lds", (long)now);
            sdec_puts(&lcd, 1, "8xGbE  2x480G SSD");
            break;
        }

        sdec_flush(&lcd);
        usleep(250000);
    }

    sdec_puts(&lcd, 0, "  Monitor stopped");
    sdec_puts(&lcd, 1, "");
    sdec_flush(&lcd);

    return 0;
}

int main(int argc, char **argv)
{
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    /* Open I/O ports */
    if (sdec_open(&lcd) < 0)
        return 1;

    int ret = 0;

    if (strcmp(cmd, "init") == 0) {
        sdec_init(&lcd);
        printf("Display initialized.\n");

    } else if (strcmp(cmd, "clear") == 0) {
        sdec_init(&lcd);
        sdec_clear(&lcd);
        sdec_flush(&lcd);
        printf("Display cleared.\n");

    } else if (strcmp(cmd, "text") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: text <0|1> <string>\n");
            ret = 1;
        } else {
            sdec_init(&lcd);
            int row = atoi(argv[2]);
            /* Join remaining args */
            char text[64] = {0};
            for (int i = 3; i < argc; i++) {
                if (i > 3) strcat(text, " ");
                strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
            }
            sdec_puts(&lcd, row, text);
            sdec_flush(&lcd);
            printf("Row %d: %s\n", row, text);
        }

    } else if (strcmp(cmd, "write") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: write <line1> [line2]\n");
            ret = 1;
        } else {
            sdec_init(&lcd);
            sdec_puts(&lcd, 0, argv[2]);
            if (argc > 3) sdec_puts(&lcd, 1, argv[3]);
            sdec_flush(&lcd);
        }

    } else if (strcmp(cmd, "btn") == 0) {
        printf("Polling buttons (Ctrl+C to stop)...\n");
        while (running) {
            uint8_t btn;
            sdec_btn_read(&lcd, &btn);
            if (btn) {
                printf("Button: 0x%02X (%s)\n", btn, sdec_btn_name(btn));
            }
            usleep(100000);
        }

    } else if (strcmp(cmd, "backlight") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: backlight <on|off>\n"); ret = 1; }
        else {
            sdec_backlight(&lcd, strcmp(argv[2], "on") == 0);
            printf("Backlight %s\n", argv[2]);
        }

    } else if (strcmp(cmd, "demo") == 0) {
        ret = cmd_demo();

    } else if (strcmp(cmd, "probe") == 0) {
        ret = cmd_probe();

    } else if (strcmp(cmd, "monitor") == 0) {
        ret = cmd_monitor();

    } else if (strcmp(cmd, "raw-cmd") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: raw-cmd <hex>\n"); ret = 1; }
        else {
            sdec_init(&lcd);
            uint8_t val = strtol(argv[2], NULL, 16);
            sdec_cmd(&lcd, val);
            printf("Sent command: 0x%02X\n", val);
        }

    } else if (strcmp(cmd, "raw-data") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: raw-data <hex>\n"); ret = 1; }
        else {
            sdec_init(&lcd);
            uint8_t val = strtol(argv[2], NULL, 16);
            sdec_data(&lcd, val);
            printf("Sent data: 0x%02X\n", val);
        }

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        ret = 1;
    }

    sdec_close(&lcd);
    return ret;
}
