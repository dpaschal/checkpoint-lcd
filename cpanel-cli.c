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
#include <termios.h>
#include <sys/utsname.h>
#ifdef __FreeBSD__
#include <sys/sysctl.h>
#endif

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
        "Buttons: ?=help LEFT RIGHT UP DOWN ENTER ESC\n"
        "Default device: %s\n"
        "Display: 16x8 characters @ 115200 baud\n", a, CPANEL_DEFAULT_DEV);
}

/* ── System info helpers ───────────────────────────────────────────── */

static long get_uptime(void)
{
#ifdef __FreeBSD__
    struct timeval boottime;
    size_t len = sizeof(boottime);
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    if (sysctl(mib, 2, &boottime, &len, NULL, 0) == 0)
        return time(NULL) - boottime.tv_sec;
#else
    FILE *f = fopen("/proc/uptime", "r");
    if (f) { double up; if (fscanf(f, "%lf", &up) == 1) { fclose(f); return (long)up; } fclose(f); }
#endif
    return 0;
}

static void get_cpu_usage(char *out, int sz)
{
    FILE *f = popen("top -b -d1 -s1 2>/dev/null | grep '^CPU:'", "r");
    if (f) {
        char line[128];
        if (fgets(line, sizeof(line), f)) {
            /* Extract idle percentage */
            char *idle = strstr(line, "idle");
            if (idle) {
                /* Walk back to find the number */
                char *p = idle - 1;
                while (p > line && *p == ' ') p--;
                while (p > line && *p != ' ' && *p != ',') p--;
                float pct = 100.0f - strtof(p, NULL);
                snprintf(out, sz, "CPU: %.0f%%", pct);
            } else {
                snprintf(out, sz, "CPU: ???");
            }
        } else {
            snprintf(out, sz, "CPU: N/A");
        }
        pclose(f);
    } else {
        snprintf(out, sz, "CPU: N/A");
    }
}

static void get_mem_usage(char *out, int sz)
{
#ifdef __FreeBSD__
    unsigned long phys = 0, user = 0;
    size_t len = sizeof(phys);
    sysctlbyname("hw.physmem", &phys, &len, NULL, 0);
    len = sizeof(user);
    sysctlbyname("hw.usermem", &user, &len, NULL, 0);
    unsigned long used_mb = (phys - user) / (1024 * 1024);
    unsigned long total_mb = phys / (1024 * 1024);
    snprintf(out, sz, "Mem:%luM/%luM", used_mb, total_mb);
#else
    snprintf(out, sz, "Mem: N/A");
#endif
}

static void get_wan_ip(char *out, int sz)
{
    FILE *f = popen("ifconfig em0 2>/dev/null | grep 'inet ' | awk '{print $2}'", "r");
    if (f) {
        char ip[32] = {0};
        if (fgets(ip, sizeof(ip), f)) {
            ip[strcspn(ip, "\n")] = 0;
            snprintf(out, sz, "WAN:%s", ip);
        } else {
            snprintf(out, sz, "WAN: down");
        }
        pclose(f);
    } else {
        snprintf(out, sz, "WAN: N/A");
    }
}

static void get_lan_ip(char *out, int sz)
{
    FILE *f = popen("ifconfig bridge0 2>/dev/null | grep 'inet ' | awk '{print $2}'", "r");
    if (f) {
        char ip[32] = {0};
        if (fgets(ip, sizeof(ip), f)) {
            ip[strcspn(ip, "\n")] = 0;
            snprintf(out, sz, "LAN:%s", ip);
        } else {
            snprintf(out, sz, "LAN: down");
        }
        pclose(f);
    } else {
        snprintf(out, sz, "LAN: N/A");
    }
}

static int get_iface_count(void)
{
    int count = 0;
    FILE *f = popen("ifconfig -l", "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f)) {
            for (char *tok = strtok(line, " \t\n"); tok; tok = strtok(NULL, " \t\n"))
                if (strncmp(tok, "em", 2) == 0) count++;
        }
        pclose(f);
    }
    return count;
}

static void get_iface_traffic(const char *iface, char *out, int sz)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "netstat -ibn -I %s 2>/dev/null | tail -1", iface);
    FILE *f = popen(cmd, "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f)) {
            unsigned long ipkts = 0, ibytes = 0, opkts = 0, obytes = 0;
            char name[16], network[32], addr[32];
            if (sscanf(line, "%15s %*d %31s %31s %lu %*d %*d %lu %lu %*d %lu",
                       name, network, addr, &ipkts, &ibytes, &opkts, &obytes) >= 7) {
                /* Convert to human-readable */
                if (ibytes > 1073741824UL)
                    snprintf(out, sz, "%s %.1fG/%.1fG", iface,
                             ibytes/1073741824.0, obytes/1073741824.0);
                else if (ibytes > 1048576UL)
                    snprintf(out, sz, "%s %luM/%luM", iface,
                             ibytes/1048576, obytes/1048576);
                else
                    snprintf(out, sz, "%s %luK/%luK", iface,
                             ibytes/1024, obytes/1024);
            } else {
                snprintf(out, sz, "%s ---", iface);
            }
        }
        pclose(f);
    }
}

static int get_pf_states(void)
{
    FILE *f = popen("pfctl -si 2>/dev/null | grep 'current entries'", "r");
    int states = 0;
    if (f) {
        char line[128];
        if (fgets(line, sizeof(line), f)) {
            char *p = strstr(line, "current entries");
            if (p) states = atoi(p + 15);
        }
        pclose(f);
    }
    return states;
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

    cpanel_puts(&lcd, 1, "  Check Point");
    cpanel_puts(&lcd, 2, "    P-210");
    cpanel_puts(&lcd, 3, "  LCD Driver");
    cpanel_puts(&lcd, 5, " Reverse Eng'd");
    cpanel_puts(&lcd, 6, "paschal + Claude");
    cpanel_puts(&lcd, 7, "  March 2026");
    cpanel_flush(&lcd);
    sleep(3);

    cpanel_puts(&lcd, 0, "=== Hardware ===");
    cpanel_puts(&lcd, 1, "CPU:  i5-750");
    cpanel_puts(&lcd, 2, "RAM:  9216 MB");
    cpanel_puts(&lcd, 3, "SSD:  2x480GB");
    cpanel_puts(&lcd, 4, "NIC:  8x 1GbE");
    cpanel_puts(&lcd, 5, "LCD:  EZIO-G500");
    cpanel_puts(&lcd, 6, "Serial 115200");
    cpanel_puts(&lcd, 7, "16 col x 8 row");
    lcd.dirty = 1;
    cpanel_flush(&lcd);
    sleep(3);

    for (int pct = 0; pct <= 100 && running; pct += 2) {
        cpanel_puts(&lcd, 0, "");
        cpanel_puts(&lcd, 1, "");
        cpanel_puts(&lcd, 2, "  Loading...");
        cpanel_puts(&lcd, 3, "");
        char bar[CPANEL_COLS + 1];
        int filled = pct * 14 / 100;
        bar[0] = '[';
        for (int j = 1; j <= 14; j++)
            bar[j] = (j <= filled) ? '#' : '-';
        bar[15] = ']';
        bar[16] = '\0';
        cpanel_puts(&lcd, 4, bar);
        cpanel_printf(&lcd, 5, "    %3d%%", pct);
        cpanel_puts(&lcd, 6, "");
        cpanel_puts(&lcd, 7, "");
        if (pct == 0) lcd.dirty = 1;
        cpanel_flush(&lcd);
        usleep(50000);
    }
    sleep(1);

    cpanel_puts(&lcd, 0, "");
    cpanel_puts(&lcd, 1, "");
    cpanel_puts(&lcd, 2, " Driver Ready!");
    cpanel_puts(&lcd, 3, "");
    cpanel_puts(&lcd, 4, " OPNsense +");
    cpanel_puts(&lcd, 5, " LCD = Win");
    cpanel_puts(&lcd, 6, "");
    cpanel_puts(&lcd, 7, "");
    lcd.dirty = 1;
    cpanel_flush(&lcd);

    return 0;
}

static int cmd_status(void)
{
    struct utsname u;
    uname(&u);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    long up = get_uptime();
    char cpu[17], mem[17], wan[17], lan[17];
    get_cpu_usage(cpu, sizeof(cpu));
    get_mem_usage(mem, sizeof(mem));
    get_wan_ip(wan, sizeof(wan));
    get_lan_ip(lan, sizeof(lan));

    cpanel_init(&lcd);
    cpanel_printf(&lcd, 0, "%-16.16s", u.nodename);
    cpanel_printf(&lcd, 1, "%02d:%02d  Up%ldd%02ldh",
                  tm->tm_hour, tm->tm_min,
                  up / 86400, (up % 86400) / 3600);
    cpanel_puts(&lcd, 2, "OPNsense 23.1.6");
    cpanel_printf(&lcd, 3, "%-8s %-7s", cpu, mem);
    cpanel_puts(&lcd, 4, wan);
    cpanel_puts(&lcd, 5, lan);
    cpanel_printf(&lcd, 6, "PF states: %d", get_pf_states());
    cpanel_printf(&lcd, 7, "NICs: %d x GbE", get_iface_count());
    cpanel_flush(&lcd);
    return 0;
}

static int cmd_monitor(void)
{
    cpanel_init(&lcd);

    int page = 0;
    int num_pages = 5;
    time_t last_auto = time(NULL);
    int auto_cycle = 0;
    time_t last_input = 0;

    while (running) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);

        /* Auto-cycle only if no recent button input */
        if (auto_cycle && now - last_auto >= 8) {
            page = (page + 1) % num_pages;
            last_auto = now;
            lcd.dirty = 1;
        }

        /* Resume auto-cycle after 30 seconds of no input */
        if (!auto_cycle && last_input && now - last_input >= 30) {
            auto_cycle = 1;
            last_auto = now;
        }

        int btn = cpanel_btn_poll(&lcd, 200);
        if (btn) {
            auto_cycle = 0;  /* stop auto-cycling on any button */
            last_input = now;
            switch (btn) {
            case CPANEL_BTN_RIGHT:
            case CPANEL_BTN_DOWN:
                page = (page + 1) % num_pages;
                break;
            case CPANEL_BTN_LEFT:
            case CPANEL_BTN_UP:
                page = (page + num_pages - 1) % num_pages;
                break;
            case CPANEL_BTN_ESC:
                running = 0;
                break;
            }
            while (cpanel_btn_poll(&lcd, 150)) ;
            lcd.dirty = 1;
        }

        switch (page) {
        case 0: {
            /* Overview */
            struct utsname u;
            uname(&u);
            long up = get_uptime();
            char cpu[17], mem[17];
            get_cpu_usage(cpu, sizeof(cpu));
            get_mem_usage(mem, sizeof(mem));

            cpanel_printf(&lcd, 0, "%-16.16s", u.nodename);
            cpanel_printf(&lcd, 1, "%02d:%02d:%02d  %02d/%02d",
                          tm->tm_hour, tm->tm_min, tm->tm_sec,
                          tm->tm_mon + 1, tm->tm_mday);
            cpanel_printf(&lcd, 2, "Up %ldd %02ldh %02ldm",
                          up/86400, (up%86400)/3600, (up%3600)/60);
            cpanel_puts(&lcd, 3, "OPNsense 23.1.6");
            cpanel_printf(&lcd, 4, "%-16s", cpu);
            cpanel_printf(&lcd, 5, "%-16s", mem);
            cpanel_printf(&lcd, 6, "PF: %d states", get_pf_states());
            cpanel_printf(&lcd, 7, " %c  Page 1/%d   ", "|/-\\"[(int)now%4], num_pages);
            break;
        }
        case 1: {
            /* Network */
            char wan[17], lan[17];
            get_wan_ip(wan, sizeof(wan));
            get_lan_ip(lan, sizeof(lan));

            cpanel_puts(&lcd, 0, "=== Network ====");
            cpanel_printf(&lcd, 1, "%-16s", wan);
            cpanel_printf(&lcd, 2, "%-16s", lan);
            cpanel_puts(&lcd, 3, "");

            char traf[17];
            get_iface_traffic("em0", traf, sizeof(traf));
            cpanel_printf(&lcd, 4, "%-16s", traf);
            cpanel_puts(&lcd, 5, "GW:192.168.50.1");
            cpanel_puts(&lcd, 6, "DNS:1.1.1.1");
            cpanel_printf(&lcd, 7, " %c  Page 2/%d   ", "|/-\\"[(int)now%4], num_pages);
            break;
        }
        case 2: {
            /* Interfaces */
            cpanel_puts(&lcd, 0, "=== Interfaces =");
            const char *names[] = {"em0","em1","em2","em3","em4","em5"};
            for (int i = 0; i < 6; i++) {
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "ifconfig %s 2>/dev/null | grep -c 'status: active'", names[i]);
                FILE *f = popen(cmd, "r");
                int up = 0;
                if (f) { char c; if (fread(&c, 1, 1, f) == 1) up = (c == '1'); pclose(f); }
                cpanel_printf(&lcd, i + 1, "%s %-11s", names[i], up ? "UP" : "down");
            }
            cpanel_printf(&lcd, 7, " %c  Page 3/%d   ", "|/-\\"[(int)now%4], num_pages);
            break;
        }
        case 3:
            /* Hardware */
            cpanel_puts(&lcd, 0, "=== Hardware ===");
            cpanel_puts(&lcd, 1, "i5-750  4 cores");
            cpanel_puts(&lcd, 2, "2.67GHz  LGA1156");
            cpanel_puts(&lcd, 3, "RAM:  9216 MB");
            cpanel_puts(&lcd, 4, "2x Samsung 480G");
            cpanel_puts(&lcd, 5, "8x Intel 82574L");
            cpanel_puts(&lcd, 6, "LCD: EZIO-G500");
            cpanel_printf(&lcd, 7, " %c  Page 4/%d   ", "|/-\\"[(int)now%4], num_pages);
            break;
        case 4:
            /* Help */
            cpanel_puts(&lcd, 0, "=== Controls ===");
            cpanel_puts(&lcd, 1, "LEFT/RIGHT  Page");
            cpanel_puts(&lcd, 2, "UP/DOWN     Page");
            cpanel_puts(&lcd, 3, "ESC         Exit");
            cpanel_puts(&lcd, 4, "ENTER  (action)");
            cpanel_puts(&lcd, 5, "?      This help");
            cpanel_puts(&lcd, 6, "Auto-rotate: 8s");
            cpanel_printf(&lcd, 7, " %c  Page 5/%d   ", "|/-\\"[(int)now%4], num_pages);
            break;
        }

        cpanel_flush(&lcd);
    }

    cpanel_clear(&lcd);
    cpanel_puts(&lcd, 3, "Monitor stopped");
    lcd.dirty = 1;
    cpanel_flush(&lcd);
    return 0;
}

static int cmd_clock(void)
{
    cpanel_init(&lcd);

    while (running) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        long up = get_uptime();

        cpanel_puts(&lcd, 0, "");
        cpanel_puts(&lcd, 1, "");
        cpanel_printf(&lcd, 2, "   %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);
        cpanel_puts(&lcd, 3, "");
        cpanel_printf(&lcd, 4, "   %04d-%02d-%02d",
                      tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
        cpanel_puts(&lcd, 5, "");
        static const char *days[] = {"Sunday","Monday","Tuesday",
            "Wednesday","Thursday","Friday","Saturday"};
        cpanel_printf(&lcd, 6, "  %s", days[tm->tm_wday]);
        cpanel_printf(&lcd, 7, "Up %ldd %02ldh %02ldm",
                      up/86400, (up%86400)/3600, (up%3600)/60);

        cpanel_flush(&lcd);

        int btn = cpanel_btn_poll(&lcd, 800);
        if (btn == CPANEL_BTN_ESC) break;
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
        lcd.dirty = 1;
        cpanel_flush(&lcd);
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
