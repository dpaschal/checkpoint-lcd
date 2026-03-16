/*
 * btn_map.c — Map all front panel buttons one at a time.
 * Beeps, shows which button on LCD, captures for 8 seconds, moves to next.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <poll.h>

static int lcd;

static void lcd_init(void) {
    uint8_t init[]={0x1B,0x40};
    write(lcd,init,2);tcdrain(lcd);usleep(100000);
    uint8_t c=0x0C;write(lcd,&c,1);tcdrain(lcd);usleep(100000);
    c=0x0B;write(lcd,&c,1);tcdrain(lcd);usleep(50000);
}

static void lcd_msg(const char *l1, const char *l2) {
    uint8_t c=0x0C;write(lcd,&c,1);tcdrain(lcd);usleep(50000);
    c=0x0B;write(lcd,&c,1);tcdrain(lcd);usleep(20000);
    write(lcd,l1,strlen(l1));tcdrain(lcd);
    c=0x0A;write(lcd,&c,1);tcdrain(lcd);
    write(lcd,l2,strlen(l2));tcdrain(lcd);
}

static void beep(void) {
    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
}

static uint8_t capture_button(const char *name, int secs) {
    char l1[17], l2[17];
    snprintf(l1, sizeof(l1), "Hold: %-10s", name);
    snprintf(l2, sizeof(l2), "NOW! %d seconds", secs);

    beep();
    lcd_msg(l1, l2);

    /* Flush any stale serial data */
    tcflush(lcd, TCIFLUSH);

    struct pollfd pfd={.fd=lcd,.events=POLLIN};
    uint8_t buf[64];
    uint8_t val = 0;
    int count = 0;

    for (int t = 0; t < secs * 20; t++) {
        int r = poll(&pfd, 1, 50);
        if (r > 0 && (pfd.revents & POLLIN)) {
            int n = read(lcd, buf, sizeof(buf));
            if (n > 0) {
                val = buf[0];
                count += n;
            }
        }
    }

    return count > 0 ? val : 0;
}

int main(void) {
    lcd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    if (lcd < 0) { perror("open"); return 1; }
    struct termios tio;
    tcgetattr(lcd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(lcd, TCSANOW, &tio);
    tcflush(lcd, TCIOFLUSH);
    lcd_init();

    const char *buttons[] = {"ESC", "UP", "DOWN", "LEFT", "RIGHT", "ENTER", "MENU", NULL};
    uint8_t values[7] = {0};

    fprintf(stderr, "=== Button Mapping ===\n");
    fprintf(stderr, "Watch LCD, hold each button when prompted.\n\n");

    for (int i = 0; buttons[i]; i++) {
        /* Countdown */
        char msg[17];
        snprintf(msg, sizeof(msg), "Next: %-10s", buttons[i]);
        lcd_msg(msg, "Get ready...");
        sleep(3);

        values[i] = capture_button(buttons[i], 8);
        fprintf(stderr, "%-8s = 0x%02X (%c)\n", buttons[i],
                values[i], (values[i] >= 0x20 && values[i] < 0x7F) ? values[i] : '.');
    }

    /* Show results on LCD */
    lcd_msg("MAPPING COMPLETE", "Check console!");
    beep(); usleep(200000); beep();

    fprintf(stderr, "\n=== BUTTON MAP ===\n");
    for (int i = 0; buttons[i]; i++) {
        if (values[i])
            fprintf(stderr, "#define BTN_%-6s 0x%02X  /* '%c' */\n",
                    buttons[i], values[i], values[i]);
        else
            fprintf(stderr, "%-8s = NOT DETECTED\n", buttons[i]);
    }

    close(lcd);
    return 0;
}
