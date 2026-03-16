#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <string.h>

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

    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);
    uint8_t c = 0x0C;
    write(fd, &c, 1); tcdrain(fd); usleep(100000);
    c = 0x0B;
    write(fd, &c, 1); tcdrain(fd); usleep(50000);

    /* Enter graphics mode and fill with 0xFF (all pixels on = inverted background) */
    uint8_t gfx[] = {0x1B, 0x47};  /* ESC G */
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);

    /* Send 1024 bytes of 0xFF (128x64/8 = 1024) */
    uint8_t block[64];
    memset(block, 0xFF, 64);
    for (int i = 0; i < 16; i++) {
        write(fd, block, 64);
        tcdrain(fd);
        usleep(5000);
    }

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    printf("Sent all-white in graphics mode\n");
    close(fd);
    return 0;
}
