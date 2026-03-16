#include <stdio.h>
#include <string.h>
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

    /* Build test patterns directly in send order:
     * Send order = L page0(64 bytes), R page0(64 bytes), L page1(64), R page1(64)...
     * Total = 8 pages * 2 panels * 64 bytes = 1024 bytes
     * 
     * Each 64 bytes = one page of one panel = 64 columns x 8 pixel rows
     * Byte value: each bit = one pixel vertically, LSB = top
     * 0x00 = 8 pixels all ON (white), 0xFF = all OFF (blue)
     */
    uint8_t send[1024];
    
    /* Fill all white first */
    memset(send, 0x00, 1024);
    
    /* Draw a dark border: top and bottom rows of pixels */
    for (int panel = 0; panel < 2; panel++) {
        int base = panel * 64; /* offset within each pair */
        /* Page 0 (top 8 rows): set bit 0 and 1 = top 2 pixel rows dark */
        for (int col = 0; col < 64; col++)
            send[base + col] = 0x03; /* bits 0,1 set = top 2 pixels dark */
        /* Page 7 (bottom 8 rows): set bits 6,7 = bottom 2 pixels dark */
        for (int col = 0; col < 64; col++)
            send[7 * 128 + base + col] = 0xC0;
    }

    /* Init + graphics mode */
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);

    /* Send in interleaved order */
    write(fd, send, 1024);
    tcdrain(fd);

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }
    printf("Sent white screen with dark border\n");
    close(fd);
    return 0;
}
