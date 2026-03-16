#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <poll.h>

static const uint8_t font_A[5] = {0x7E,0x11,0x11,0x11,0x7E};

static void send_gfx(int fd, uint8_t *buf) {
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(200000);
    uint8_t clr = 0x0C; write(fd, &clr, 1); tcdrain(fd); usleep(100000);
    uint8_t hm = 0x0B; write(fd, &hm, 1); tcdrain(fd); usleep(50000);
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);
    /* Method 1: interleaved L/R pages, no delays */
    for (int p = 0; p < 8; p++) {
        write(fd, &buf[p*64], 64);
        write(fd, &buf[512+p*64], 64);
    }
    tcdrain(fd);
}

int main(void) {
    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    uint8_t buf[1024];

    /* Step 1: Normal A — letter pixels ON, rest OFF */
    memset(buf, 0x00, 1024);
    for (int c = 0; c < 5; c++)
        buf[c] = font_A[c];  /* left panel, page 0, cols 0-4 */
    send_gfx(fd, buf);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Step 1: Normal A. Press any front panel button.\n");

    struct pollfd pfd = {.fd=fd, .events=POLLIN};
    for (int i = 0; i < 100; i++) {
        if (poll(&pfd, 1, 100) > 0) { uint8_t b; read(fd, &b, 1); break; }
    }

    /* Step 2: Inverted — all ON, letter pixels OFF */
    memset(buf, 0xFF, 1024);
    for (int c = 0; c < 5; c++)
        buf[c] = ~font_A[c];
    send_gfx(fd, buf);

    sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8CC", 6); close(sp); }
    printf("Step 2: Inverted A. Done.\n");

    close(fd);
    return 0;
}
