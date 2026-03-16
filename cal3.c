#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv) {
    int cols = argc > 1 ? atoi(argv[1]) : 16;
    int rows = argc > 2 ? atoi(argv[2]) : 4;

    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    /* FULL reset: ESC @ + clear + home */
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(200000);
    uint8_t c = 0x0C;
    write(fd, &c, 1); tcdrain(fd); usleep(200000);
    c = 0x0B;
    write(fd, &c, 1); tcdrain(fd); usleep(100000);

    printf("Testing %dx%d (no newlines, full init)\n", cols, rows);
    
    for (int r = 0; r < rows; r++) {
        char line[32];
        memset(line, '.', cols);
        int n = snprintf(line, 4, "R%d:", r);
        line[n] = '.'; /* overwrite null from snprintf */
        line[cols - 1] = '0' + r;
        write(fd, line, cols);
        tcdrain(fd);
        usleep(3000);
    }

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Done.\n");
    close(fd);
    return 0;
}
