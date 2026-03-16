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

    uint8_t c;
    uint8_t init[]={0x1B,0x40};
    write(fd,init,2);tcdrain(fd);usleep(100000);
    c=0x0C;write(fd,&c,1);tcdrain(fd);usleep(100000);
    c=0x0B;write(fd,&c,1);tcdrain(fd);usleep(50000);

    char *rows[] = {
        "ROW0-AAAAAAAAA\n",
        "ROW1-BBBBBBBBB\n",
        "ROW2-CCCCCCCCC\n",
        "ROW3-DDDDDDDDD\n",
        "ROW4-EEEEEEEEE\n",
        "ROW5-FFFFFFFFF\n",
        "ROW6-GGGGGGGGG\n",
        "ROW7-HHHHHHHHH",
    };
    for (int i = 0; i < 8; i++) {
        write(fd, rows[i], strlen(rows[i]));
        tcdrain(fd);
        usleep(5000);
    }
    printf("Sent. Read the LCD top-to-bottom and tell me the row order.\n");
    close(fd);
    return 0;
}
