#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>

/* 5x8 font for 'A' */
static const uint8_t font_A[5] = {0x7E,0x11,0x11,0x11,0x7E};

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

    /* === STEP 1: Normal 'A' at 0,0 (dark letter on blank background) === */
    memset(buf, 0x00, 1024);  /* all pixels off */
    /* Draw A: 5 columns, each byte = 8 vertical pixels */
    for (int c = 0; c < 5; c++)
        buf[c] = font_A[c];  /* page 0, columns 0-4 of left panel */

    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(200000);
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);
    write(fd, buf, 1024); tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Step 1: Normal A. Press ENTER to see inverted.\n");
    
    /* Wait for button press */
    struct pollfd pfd = {.fd=fd, .events=POLLIN};
    for (int i = 0; i < 100; i++) {
        if (poll(&pfd, 1, 100) > 0) { uint8_t b; read(fd, &b, 1); break; }
    }

    /* === STEP 2: Inverted 'A' (light letter on filled background) === */
    memset(buf, 0xFF, 1024);  /* all pixels on */
    for (int c = 0; c < 5; c++)
        buf[c] = 0xFF ^ font_A[c];  /* invert the letter pixels */

    write(fd, init, 2); tcdrain(fd); usleep(200000);
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);
    write(fd, buf, 1024); tcdrain(fd);

    if (sp >= 0) { sp = open("/dev/speaker", O_WRONLY); write(sp, "O2L8CC", 6); close(sp); }
    printf("Step 2: Inverted A. Done.\n");

    close(fd);
    return 0;
}
