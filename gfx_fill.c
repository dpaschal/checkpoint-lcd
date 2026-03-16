#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

int main(int argc, char **argv) {
    uint8_t val = argc > 1 ? strtol(argv[1], NULL, 16) : 0x00;

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
    memset(buf, val, 1024);

    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(200000);
    uint8_t c = 0x0C;
    write(fd, &c, 1); tcdrain(fd); usleep(100000);
    c = 0x0B;
    write(fd, &c, 1); tcdrain(fd); usleep(50000);
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(100000);

    /* Method 1: interleaved L/R */
    for (int p = 0; p < 8; p++) {
        write(fd, &buf[p*64], 64);
        write(fd, &buf[512+p*64], 64);
    }
    tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Filled with 0x%02X\n", val);
    close(fd);
    return 0;
}
