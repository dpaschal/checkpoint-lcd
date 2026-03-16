#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

static int fd;

static void lcd_init(void) {
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);
}

static void lcd_gfx(void) {
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);
}

static void beep(void) {
    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
}

/* Make a test pattern: top half white (0x00), bottom half blue (0xFF) 
 * If the ordering is correct, we'll see a clear horizontal split */
static void make_pattern(uint8_t *buf, int method) {
    /* Start with all blue (0xFF = pixels off) */
    memset(buf, 0xFF, 1024);

    switch (method) {
    case 0:
        /* Method 0: Linear. Pages 0-3 = white (top half), 4-7 = blue */
        memset(buf, 0x00, 512);
        break;
    case 1:
        /* Method 1: Interleaved L/R. Even positions = left, odd = right */
        /* Top half: pages 0-3 of both panels */
        for (int p = 0; p < 4; p++) {
            memset(&buf[p * 128], 0x00, 64);        /* left */
            memset(&buf[p * 128 + 64], 0x00, 64);   /* right */
        }
        break;
    case 2:
        /* Method 2: Left panel all pages, then right panel all pages */
        /* Top half of left panel (pages 0-3) */
        memset(&buf[0], 0x00, 256);
        /* Top half of right panel (pages 0-3) */
        memset(&buf[512], 0x00, 256);
        break;
    case 3:
        /* Method 3: Alternating left page / right page */
        for (int p = 0; p < 4; p++) {
            memset(&buf[p * 128], 0x00, 64);       /* left page p */
            memset(&buf[p * 128 + 64], 0x00, 64);  /* right page p */
        }
        break;
    case 4:
        /* Method 4: Just fill first 512 bytes with checkerboard */
        for (int i = 0; i < 1024; i++)
            buf[i] = (i < 512) ? 0x55 : 0xAA;
        break;
    case 5:
        /* Method 5: Vertical stripes - every other column white */
        for (int i = 0; i < 1024; i++)
            buf[i] = (i % 2 == 0) ? 0x00 : 0xFF;
        break;
    }
}

static void send_fb(uint8_t *buf, int method) {
    lcd_init();
    lcd_gfx();

    switch (method) {
    case 0: /* Linear dump */
        write(fd, buf, 1024);
        break;
    case 1: /* Interleaved: L page0, R page0, L page1, R page1... */
        for (int p = 0; p < 8; p++) {
            write(fd, &buf[p * 64], 64);
            tcdrain(fd); usleep(2000);
            write(fd, &buf[512 + p * 64], 64);
            tcdrain(fd); usleep(2000);
        }
        break;
    case 2: /* Interleaved reversed: R page0, L page0, R page1... */
        for (int p = 0; p < 8; p++) {
            write(fd, &buf[512 + p * 64], 64);
            tcdrain(fd); usleep(2000);
            write(fd, &buf[p * 64], 64);
            tcdrain(fd); usleep(2000);
        }
        break;
    case 3: /* All left then all right */
        write(fd, buf, 512);
        tcdrain(fd); usleep(5000);
        write(fd, buf + 512, 512);
        break;
    case 4: /* All right then all left */
        write(fd, buf + 512, 512);
        tcdrain(fd); usleep(5000);
        write(fd, buf, 512);
        break;
    }
    tcdrain(fd);
}

int main(int argc, char **argv) {
    int pattern = argc > 1 ? atoi(argv[1]) : 0;
    int send_method = argc > 2 ? atoi(argv[2]) : 0;

    fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    uint8_t buf[1024];
    make_pattern(buf, pattern);
    send_fb(buf, send_method);

    beep();
    printf("Pattern %d, send method %d\n", pattern, send_method);
    close(fd);
    return 0;
}
