#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

int main(int argc, char **argv) {
    int method = argc > 1 ? atoi(argv[1]) : 0;

    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    /* Half screen pattern: first half 0x55 (checkerboard), second half 0xAA */
    uint8_t buf[1024];
    memset(buf, 0x55, 512);
    memset(buf+512, 0xAA, 512);

    /* Init + gfx mode - try WITHOUT ESC @ first, just ESC G directly */
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(200000);
    
    /* Clear and home first */
    uint8_t c = 0x0C;
    write(fd, &c, 1); tcdrain(fd); usleep(100000);
    c = 0x0B;
    write(fd, &c, 1); tcdrain(fd); usleep(50000);
    
    /* Enter graphics mode */
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(100000);

    switch(method) {
    case 0: /* Linear */
        printf("M0: Linear 1024 bytes\n");
        write(fd, buf, 1024); break;
    case 1: /* L page, R page alternating */
        printf("M1: L/R interleaved\n");
        for (int p = 0; p < 8; p++) {
            write(fd, &buf[p*64], 64);
            write(fd, &buf[512+p*64], 64);
        } break;
    case 2: /* R page, L page alternating */
        printf("M2: R/L interleaved\n");
        for (int p = 0; p < 8; p++) {
            write(fd, &buf[512+p*64], 64);
            write(fd, &buf[p*64], 64);
        } break;
    case 3: /* All left then all right */
        printf("M3: All left then right\n");
        write(fd, buf, 512);
        write(fd, buf+512, 512); break;
    case 4: /* All right then all left */
        printf("M4: All right then left\n");
        write(fd, buf+512, 512);
        write(fd, buf, 512); break;
    case 5: /* Try 128 bytes per page (full width) */
        printf("M5: 128 bytes per page x 8\n");
        for (int p = 0; p < 8; p++) {
            write(fd, &buf[p*128], 128);
            tcdrain(fd); usleep(2000);
        } break;
    case 6: /* Try NO init, just ESC G + data */
        printf("M6: ESC G only, no ESC @\n");
        /* Re-open and send just ESC G */
        tcflush(fd, TCIOFLUSH);
        write(fd, gfx, 2); tcdrain(fd); usleep(50000);
        write(fd, buf, 1024); break;
    case 7: /* Try sending 2048 bytes (maybe display expects more?) */
        printf("M7: 2048 bytes\n");
        write(fd, buf, 1024);
        write(fd, buf, 1024); break;
    }
    tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    close(fd);
    return 0;
}
