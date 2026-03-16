#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>
#include <machine/cpufunc.h>

static volatile sig_atomic_t running = 1;
static void sh(int s) { (void)s; running = 0; }

int main(void) {
    signal(SIGINT, sh);
    int sfd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(sfd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(sfd, TCSANOW, &tio);
    tcflush(sfd, TCIOFLUSH);

    int iofd = open("/dev/io", O_RDWR);
    outb(0x2E,0x87);outb(0x2E,0x87);
    outb(0x2E,0x07);outb(0x2F,0x01);
    outb(0x2E,0x30);outb(0x2F,0x01);
    outb(0x2E,0xAA);
    usleep(10000);

    uint8_t cmd[]={0x1B,0x40};
    write(sfd,cmd,2);tcdrain(sfd);usleep(100000);
    uint8_t c=0x0C;write(sfd,&c,1);tcdrain(sfd);usleep(100000);
    c=0x0B;write(sfd,&c,1);tcdrain(sfd);usleep(50000);
    write(sfd,"PRESS ALL BUTTONS",17);tcdrain(sfd);
    c=0x0A;write(sfd,&c,1);tcdrain(sfd);
    write(sfd,"NOW! (45 seconds)",17);tcdrain(sfd);

    printf("Press ALL buttons for 45 seconds!\n\n");
    fflush(stdout);

    struct pollfd pfd={.fd=sfd,.events=POLLIN};
    uint8_t buf[64], last_lpt=inb(0x379);
    int sc=0,lc=0;

    for(int t=0;t<450&&running;t++){
        int r=poll(&pfd,1,100);
        if(r>0&&(pfd.revents&POLLIN)){
            int n=read(sfd,buf,sizeof(buf));
            if(n>0){
                sc++;
                printf("[SER %2d]",sc);
                for(int i=0;i<n;i++)
                    printf(" 0x%02X(%c)",buf[i],(buf[i]>=0x20&&buf[i]<0x7F)?buf[i]:'.');
                printf("\n");
                fflush(stdout);
            }
        }
        uint8_t lpt=inb(0x379);
        if(lpt!=last_lpt){
            lc++;
            printf("[LPT %2d] 0x%02X->0x%02X\n",lc,last_lpt,lpt);
            fflush(stdout);
            last_lpt=lpt;
        }
    }
    printf("\nSerial:%d LPT:%d\n",sc,lc);
    close(iofd);close(sfd);return 0;
}
