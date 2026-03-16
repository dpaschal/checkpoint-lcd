#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

int main(int argc, char **argv) {
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

    /* Try multiple possible invert commands */
    uint8_t cmds[][2] = {
        {0x1B, 0x52},  /* ESC R - reverse? */
        {0x1B, 0x72},  /* ESC r - reverse? */
        {0x1B, 0x69},  /* ESC i - invert? */
        {0x1B, 0x49},  /* ESC I - invert? */
    };

    int try = 0;
    if (argc > 1) try = atoi(argv[1]);

    if (try < 4) {
        printf("Trying: ESC 0x%02X\n", cmds[try][1]);
        write(fd, cmds[try], 2); tcdrain(fd); usleep(50000);
    }

    write(fd, "Invert test!", 12); tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    close(fd);
    return 0;
}
