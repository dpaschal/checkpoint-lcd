#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

int main(int argc, char **argv) {
    int block = argc > 1 ? atoi(argv[1]) : 0;
    /* block 0-15: illuminate one 64-byte block at a time */

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
    memset(buf, 0x00, 1024); /* all blue/off */
    
    /* Light up just one 64-byte block */
    if (block >= 0 && block < 16)
        memset(&buf[block * 64], 0xFF, 64); /* white/on */

    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);

    /* Send linearly */
    write(fd, buf, 1024);
    tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Block %d lit (bytes %d-%d)\n", block, block*64, block*64+63);
    close(fd);
    return 0;
}
