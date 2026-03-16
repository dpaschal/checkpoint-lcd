#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <poll.h>
#include <machine/cpufunc.h>

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

    /* Enable LPT */
    int iofd = open("/dev/io", O_RDWR);
    outb(0x2E,0x87);outb(0x2E,0x87);
    outb(0x2E,0x07);outb(0x2F,0x01);
    outb(0x2E,0x30);outb(0x2F,0x01);
    outb(0x2E,0xAA);
    usleep(10000);

    /* Show message */
    uint8_t init[]={0x1B,0x40};
    write(fd,init,2);tcdrain(fd);usleep(100000);
    uint8_t c=0x0C;write(fd,&c,1);tcdrain(fd);usleep(100000);
    c=0x0B;write(fd,&c,1);tcdrain(fd);usleep(50000);
    write(fd,"Press ESC button",16);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"and HOLD it down",16);tcdrain(fd);

    fprintf(stderr,"=== ESC Button Test ===\n");
    fprintf(stderr,"Sampling serial + LPT + all 3 LPT ports every 50ms\n");
    fprintf(stderr,"Press and HOLD the ESC button.\n\n");

    /* Record baseline of ALL ports */
    uint8_t base_data = inb(0x378);
    uint8_t base_stat = inb(0x379);
    uint8_t base_ctrl = inb(0x37A);
    fprintf(stderr,"Baseline: data=0x%02X status=0x%02X ctrl=0x%02X\n\n", 
            base_data, base_stat, base_ctrl);

    struct pollfd pfd={.fd=fd,.events=POLLIN};
    uint8_t buf[64];
    int count=0;

    for(int t=0;t<600;t++){  /* 30 seconds */
        /* Check serial */
        int r=poll(&pfd,1,50);
        if(r>0&&(pfd.revents&POLLIN)){
            int n=read(fd,buf,sizeof(buf));
            if(n>0){
                count++;
                fprintf(stderr,"[SERIAL %2d]",count);
                for(int i=0;i<n;i++) fprintf(stderr," 0x%02X",buf[i]);
                fprintf(stderr,"\n");
            }
        }

        /* Check ALL LPT ports for ANY change */
        uint8_t d=inb(0x378), s=inb(0x379), cv=inb(0x37A);
        if(d!=base_data || s!=base_stat || cv!=base_ctrl){
            count++;
            fprintf(stderr,"[LPT    %2d] data:0x%02X%s stat:0x%02X%s ctrl:0x%02X%s\n",
                count,
                d, d!=base_data?" *CHANGED*":"",
                s, s!=base_stat?" *CHANGED*":"",
                cv, cv!=base_ctrl?" *CHANGED*":"");
            /* Update baseline so we see the release too */
            base_data=d; base_stat=s; base_ctrl=cv;
        }
    }
    fprintf(stderr,"\nTotal events: %d\n",count);
    close(iofd);close(fd);return 0;
}
