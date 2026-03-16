#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <poll.h>

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

    uint8_t init[]={0x1B,0x40};
    write(fd,init,2);tcdrain(fd);usleep(100000);
    uint8_t c=0x0C;write(fd,&c,1);tcdrain(fd);usleep(100000);
    c=0x0B;write(fd,&c,1);tcdrain(fd);usleep(50000);
    write(fd,"Hold ? button",13);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"NOW! 10 seconds",15);tcdrain(fd);

    /* Beep */
    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    tcflush(fd, TCIFLUSH);

    struct pollfd pfd={.fd=fd,.events=POLLIN};
    uint8_t buf[64];
    uint8_t val=0;
    int count=0;

    for (int t=0; t<200; t++) {
        int r=poll(&pfd,1,50);
        if(r>0&&(pfd.revents&POLLIN)){
            int n=read(fd,buf,sizeof(buf));
            if(n>0){ val=buf[0]; count+=n; }
        }
    }

    fprintf(stderr,"? button = 0x%02X (%c)  [%d events]\n",
            val, (val>=0x20&&val<0x7F)?val:'.', count);
    close(fd);
    return 0;
}
