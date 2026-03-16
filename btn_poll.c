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

    /* Init LCD */
    uint8_t init[]={0x1B,0x40};
    write(fd,init,2);tcdrain(fd);usleep(100000);
    uint8_t c=0x0C;write(fd,&c,1);tcdrain(fd);usleep(100000);
    c=0x0B;write(fd,&c,1);tcdrain(fd);usleep(50000);
    write(fd,"HOLD a button and",17);tcdrain(fd);
    c=0x0A;write(fd,&c,1);tcdrain(fd);
    write(fd,"watch this screen!",18);tcdrain(fd);

    printf("Polling buttons every 200ms for 30 seconds.\n");
    printf("HOLD a button down while test runs.\n\n");
    fflush(stdout);

    /* Try multiple polling approaches */
    struct pollfd pfd={.fd=fd,.events=POLLIN};
    uint8_t buf[64];
    int count=0;
    uint8_t last=0;

    for(int t=0;t<150&&running;t++){
        /* Method A: Send EZIO button query 0x75 */
        c=0x75;
        write(fd,&c,1);tcdrain(fd);

        /* Read response */
        int r=poll(&pfd,1,200);
        if(r>0&&(pfd.revents&POLLIN)){
            int n=read(fd,buf,sizeof(buf));
            if(n>0){
                for(int i=0;i<n;i++){
                    if(buf[i]!=last){
                        count++;
                        printf("[%3d] 0x%02X (%c)\n",count,buf[i],
                            (buf[i]>=0x20&&buf[i]<0x7F)?buf[i]:'.');
                        fflush(stdout);
                        last=buf[i];
                    }
                }
            }
        }
    }
    printf("\nTotal unique events: %d\n",count);
    close(fd);return 0;
}
