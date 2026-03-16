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

    /* Test 1: Write exactly 21 chars with NO newline - see where they wrap */
    write(fd, "123456789012345678901", 21);
    tcdrain(fd);
    usleep(100000);

    /* Test 2: Then write 21 more chars */
    write(fd, "ABCDEFGHIJKLMNOPQRSTU", 21);
    tcdrain(fd);

    printf("Sent 21 digits + 21 letters. Check where they appear.\n");
    close(fd);
    return 0;
}
