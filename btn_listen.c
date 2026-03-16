#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>

static volatile sig_atomic_t running = 1;
static void sh(int s) { (void)s; running = 0; }

int main(void) {
    signal(SIGINT, sh);
    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    /* Init and show message */
    uint8_t init[]={0x1B,0x40};
    write(fd,init,2);tcdrain(fd);usleep(100000);
    uint8_t c=0x0C;write(fd,&c,1);tcdrain(fd);usleep(100000);
    c=0x0B;write(fd,&c,1);tcdrain(fd);usleep(50000);
    write(fd,"BUTTON TEST",11);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"Just listening...",17);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"Press each button",17);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"one at a time.",14);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"Wait 1 sec between",18);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"each press.",11);tcdrain(fd);

    fprintf(stderr,"Passive listen - 60 sec. NO query commands sent.\n");
    fprintf(stderr,"Press one button at a time, wait 1 sec between.\n\n");

    struct pollfd pfd={.fd=fd,.events=POLLIN};
    uint8_t buf[64];
    int count=0;

    for(int t=0;t<600&&running;t++){
        int r=poll(&pfd,1,100);
        if(r>0&&(pfd.revents&POLLIN)){
            int n=read(fd,buf,sizeof(buf));
            if(n>0){
                count++;
                fprintf(stderr,"[%2d] %d bytes:",count,n);
                for(int i=0;i<n;i++)
                    fprintf(stderr," 0x%02X(%c)",buf[i],(buf[i]>=0x20&&buf[i]<0x7F)?buf[i]:'.');
                fprintf(stderr,"\n");
            }
        }
    }
    fprintf(stderr,"\nTotal: %d events\n",count);
    close(fd);return 0;
}
