#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

int main(int argc, char **argv) {
    int cols = 21, rows = 7;
    if (argc > 1) cols = atoi(argv[1]);
    if (argc > 2) rows = atoi(argv[2]);

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

    printf("Testing %d cols x %d rows\n", cols, rows);
    
    for (int r = 0; r < rows; r++) {
        char line[32];
        /* Start with row number, pad with dots, end with row number */
        int n = snprintf(line, sizeof(line), "R%d", r);
        for (int i = n; i < cols - 1; i++) line[i] = '.';
        line[cols - 1] = '0' + r;
        write(fd, line, cols);
        tcdrain(fd);
        usleep(3000);
    }

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    printf("Done. Each row starts and ends with its number.\n");
    close(fd);
    return 0;
}
