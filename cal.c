#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

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

    /* Write 8 rows of exactly 20 chars each, NO newlines */
    write(fd, "Row0:***20chars****", 20); tcdrain(fd);
    write(fd, "Row1:***20chars****", 20); tcdrain(fd);
    write(fd, "Row2:***20chars****", 20); tcdrain(fd);
    write(fd, "Row3:***20chars****", 20); tcdrain(fd);
    write(fd, "Row4:***20chars****", 20); tcdrain(fd);
    write(fd, "Row5:***20chars****", 20); tcdrain(fd);
    write(fd, "Row6:***20chars****", 20); tcdrain(fd);
    write(fd, "Row7:***20chars****", 20); tcdrain(fd);

    /* Beep */
    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    printf("Sent 8 rows x 20 chars. Check LCD.\n");
    close(fd);
    return 0;
}
