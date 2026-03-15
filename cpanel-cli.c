/*
 * cpanel-cli.c — Check Point P-210/12200 LCD Panel Tool
 *
 * Usage: cpanel [-d device] <command> [args...]
 */

#include "cpanel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/utsname.h>
#include <termios.h>

static cpanel_t lcd;
static volatile sig_atomic_t running = 1;

static void sighandler(int sig) { (void)sig; running = 0; }

static void usage(const char *a)
{
    fprintf(stderr,
        "cpanel — Check Point P-210/12200 LCD Panel Tool\n"
        "Reverse-engineered EZIO-G500 serial driver\n\n"
        "Usage: %s [-d device] <command> [args...]\n\n"
        "Commands:\n"
        "  init                  Reset display\n"
        "  clear                 Clear display\n"
        "  write <line> [...]    Write lines (up to 8)\n"
        "  text <row> <string>   Write to specific row (0-7)\n"
        "  demo                  Animated demo\n"
        "  status                System status display\n"
        "  monitor               Live updating status\n"
        "  clock                 Full-screen clock\n"
        "  raw <string>          Send raw text\n\n"
        "Default device: %s\n"
        "Display: 16x8 characters @ 115200 baud\n", a, CPANEL_DEFAULT_DEV);
}

/* ── Commands ──────────────────────────────────────────────────────── */

static int cmd_write(int argc, char **argv)
{
    cpanel_init(&lcd);
    for (int i = 0; i < argc && i < CPANEL_ROWS; i++)
        cpanel_puts(&lcd, i, argv[i]);
    cpanel_flush(&lcd);
    return 0;
}

static int cmd_text(int row, int argc, char **argv)
{
    char text[CPANEL_COLS + 1] = {0};
    for (int i = 0; i < argc; i++) {
        if (i > 0) strncat(text, " ", sizeof(text) - strlen(text) - 1);
        strncat(text, argv[i], sizeof(text) - strlen(text) - 1);
    }
    cpanel_puts(&lcd, row, text);
    cpanel_flush(&lcd);
    return 0;
}

static int cmd_demo(void)
{
    cpanel_init(&lcd);

    /* Title */
    cpanel_puts(&lcd, 1, "  Check Point");
    cpanel_puts(&lcd, 2, "    P-210");
    cpanel_puts(&lcd, 3, "  LCD Driver");
    cpanel_puts(&lcd, 5, " Reverse Eng'd");
    cpanel_puts(&lcd, 6, "paschal + Claude");
    cpanel_puts(&lcd, 7, "  March 2026");
    cpanel_flush(&lcd);
    sleep(3);

    /* Hardware info */
    cpanel_clear(&lcd);
    cpanel_puts(&lcd, 0, "=== Hardware ===");
    cpanel_puts(&lcd, 1, "CPU:  i5-750");
    cpanel_puts(&lcd, 2, "RAM:  9216 MB");
    cpanel_puts(&lcd, 3, "SSD:  2x480GB");
    cpanel_puts(&lcd, 4, "NIC:  8x 1GbE");
    cpanel_puts(&lcd, 5, "LCD:  EZIO-G500");
    cpanel_puts(&lcd, 6, "Serial 115200");
    cpanel_puts(&lcd, 7, "16 col x 8 row");
    cpanel_flush(&lcd);
    sleep(3);

    /* Progress bar */
    for (int pct = 0; pct <= 100 && running; pct += 2) {
        cpanel_clear(&lcd);
        cpanel_puts(&lcd, 2, "  Loading...");
        char bar[CPANEL_COLS + 1];
        int filled = pct * 14 / 100;
        bar[0] = '[';
        for (int j = 1; j <= 14; j++)
            bar[j] = (j <= filled) ? '#' : '-';
        bar[15] = ']';
        bar[16] = '\0';
        cpanel_puts(&lcd, 4, bar);
        cpanel_printf(&lcd, 5, "    %3d%%", pct);
        cpanel_flush(&lcd);
        usleep(50000);
    }
    sleep(1);

    /* Scroll demo */
    const char *lines[] = {
        "The quick brown",
        "fox jumps over",
        "the lazy dog.",
        "",
        "This LCD was",
        "reverse-engineered",
        "from a Check Point",
        "P-210 appliance.",
        "",
        "EZIO-G500 display",
        "128x64 pixels",
        "8x8 font = 16x8",
        "character text.",
        "",
        "Serial protocol:",
        "115200 8N1",
        "/dev/cuau1",
        "ESC @ to init",
        NULL,
    };
    for (int start = 0; lines[start] && running; start++) {
        cpanel_clear(&lcd);
        for (int r = 0; r < CPANEL_ROWS && lines[start + r]; r++)
            cpanel_puts(&lcd, r, lines[start + r]);
        cpanel_flush(&lcd);
        usleep(600000);
    }

    /* Done */
    cpanel_clear(&lcd);
    cpanel_puts(&lcd, 2, " Driver Ready!");
    cpanel_puts(&lcd, 4, " OPNsense +");
    cpanel_puts(&lcd, 5, " LCD = Win");
    cpanel_flush(&lcd);

    return 0;
}

static int cmd_status(void)
{
    struct utsname u;
    uname(&u);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    cpanel_init(&lcd);
    cpanel_printf(&lcd, 0, "%-16.16s", u.nodename);
    cpanel_printf(&lcd, 1, "%02d:%02d:%02d  %02d/%02d",
                  tm->tm_hour, tm->tm_min, tm->tm_sec,
                  tm->tm_mon + 1, tm->tm_mday);
    cpanel_puts(&lcd, 2, "OPNsense 23.1.6");
    cpanel_printf(&lcd, 3, "%-16.16s", u.release);

    /* Read uptime */
    FILE *f;
#ifdef __FreeBSD__
    /* FreeBSD: use sysctl */
    cpanel_puts(&lcd, 5, "LAN 192.168.1.1");
    cpanel_puts(&lcd, 6, "WRK 192.168.10.1");
    cpanel_puts(&lcd, 7, "8xGbE  2x480G");
#else
    f = fopen("/proc/uptime", "r");
    if (f) {
        double up;
        if (fscanf(f, "%lf", &up) == 1) {
            int days = (int)(up / 86400);
            int hours = (int)(up / 3600) % 24;
            cpanel_printf(&lcd, 4, "Up: %dd %dh", days, hours);
        }
        fclose(f);
    }
    cpanel_puts(&lcd, 5, "LAN 192.168.1.1");
    cpanel_puts(&lcd, 6, "WRK 192.168.10.1");
    cpanel_puts(&lcd, 7, "8xGbE  2x480G");
#endif

    cpanel_flush(&lcd);
    return 0;
}

static int cmd_monitor(void)
{
    cpanel_init(&lcd);

    int page = 0;
    time_t last_switch = time(NULL);

    while (running) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        if (now - last_switch >= 5) {
            page = (page + 1) % 3;
            last_switch = now;
        }

        cpanel_clear(&lcd);

        switch (page) {
        case 0: {
            struct utsname u;
            uname(&u);
            cpanel_printf(&lcd, 0, "%-16.16s", u.nodename);
            cpanel_printf(&lcd, 1, "%02d:%02d:%02d  %02d/%02d",
                          tm->tm_hour, tm->tm_min, tm->tm_sec,
                          tm->tm_mon + 1, tm->tm_mday);
            cpanel_puts(&lcd, 2, "OPNsense 23.1.6");
            cpanel_puts(&lcd, 3, "================");
            cpanel_puts(&lcd, 4, "LAN 192.168.1.1");
            cpanel_puts(&lcd, 5, "WRK 192.168.10.1");
            cpanel_puts(&lcd, 6, "WAN not connected");
            cpanel_printf(&lcd, 7, "Page 1/3  [%c]",
                          "|/-\\"[(int)now % 4]);
            break;
        }
        case 1:
            cpanel_puts(&lcd, 0, "=== Interfaces =");
            cpanel_puts(&lcd, 1, "em0 WAN    down");
            cpanel_puts(&lcd, 2, "em1 LAN    up");
            cpanel_puts(&lcd, 3, "em2 LAN    up");
            cpanel_puts(&lcd, 4, "em3 LAN    up");
            cpanel_puts(&lcd, 5, "em4 LAN    up");
            cpanel_puts(&lcd, 6, "em5 LAN    up");
            cpanel_puts(&lcd, 7, "Page 2/3");
            break;
        case 2:
            cpanel_puts(&lcd, 0, "=== Hardware ===");
            cpanel_puts(&lcd, 1, "CPU:  i5-750");
            cpanel_puts(&lcd, 2, "RAM:  9216 MB");
            cpanel_puts(&lcd, 3, "SSD:  2x 480GB");
            cpanel_puts(&lcd, 4, "NIC:  8x GbE");
            cpanel_puts(&lcd, 5, "");
            cpanel_puts(&lcd, 6, "Check Point P-210");
            cpanel_puts(&lcd, 7, "Page 3/3");
            break;
        }

        cpanel_flush(&lcd);
        sleep(1);
    }

    cpanel_clear(&lcd);
    cpanel_puts(&lcd, 3, "Monitor stopped");
    cpanel_flush(&lcd);
    return 0;
}

static int cmd_clock(void)
{
    cpanel_init(&lcd);

    while (running) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        cpanel_clear(&lcd);
        cpanel_printf(&lcd, 2, "   %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);
        cpanel_printf(&lcd, 4, "   %04d-%02d-%02d",
                      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);

        static const char *days[] = {"Sunday","Monday","Tuesday",
            "Wednesday","Thursday","Friday","Saturday"};
        cpanel_printf(&lcd, 6, "  %s", days[tm->tm_wday]);
        cpanel_flush(&lcd);
        sleep(1);
    }
    return 0;
}

/* ── Main ──────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    const char *device = NULL;
    int idx = 1;

    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);

    if (argc > 2 && strcmp(argv[1], "-d") == 0) {
        device = argv[2];
        idx = 3;
    }

    if (idx >= argc) { usage(argv[0]); return 1; }

    const char *cmd = argv[idx];
    int cargc = argc - idx - 1;
    char **cargv = &argv[idx + 1];

    if (cpanel_open(&lcd, device) < 0) return 1;

    int ret = 0;

    if (strcmp(cmd, "init") == 0) {
        cpanel_init(&lcd);
    } else if (strcmp(cmd, "clear") == 0) {
        cpanel_init(&lcd);
        cpanel_clear(&lcd);
    } else if (strcmp(cmd, "write") == 0) {
        ret = cmd_write(cargc, cargv);
    } else if (strcmp(cmd, "text") == 0) {
        if (cargc < 2) { fprintf(stderr, "Usage: text <row> <string>\n"); ret = 1; }
        else ret = cmd_text(atoi(cargv[0]), cargc - 1, &cargv[1]);
    } else if (strcmp(cmd, "demo") == 0) {
        ret = cmd_demo();
    } else if (strcmp(cmd, "status") == 0) {
        ret = cmd_status();
    } else if (strcmp(cmd, "monitor") == 0) {
        ret = cmd_monitor();
    } else if (strcmp(cmd, "clock") == 0) {
        ret = cmd_clock();
    } else if (strcmp(cmd, "raw") == 0) {
        cpanel_init(&lcd);
        for (int i = 0; i < cargc; i++) {
            (void)write(lcd.fd, cargv[i], strlen(cargv[i]));
            if (i < cargc - 1) { uint8_t nl = CPANEL_NEWLINE; (void)write(lcd.fd, &nl, 1); }
        }
        tcdrain(lcd.fd);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        usage(argv[0]);
        ret = 1;
    }

    cpanel_close(&lcd);
    return ret;
}
