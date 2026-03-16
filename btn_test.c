/*
 * btn_test.c — Test button input from the EZIO-G500 front panel
 * Reads serial data from /dev/cuau1 while displaying prompts.
 * Press buttons and we'll capture what bytes come back.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>

static volatile sig_atomic_t running = 1;
static void sighandler(int s) { (void)s; running = 0; }

int main(void) {
    signal(SIGINT, sighandler);

    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open cuau1"); return 1; }

    struct termios tio;
    tcgetattr(fd, &tio);
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    /* Init display */
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);
    uint8_t clr = 0x0C;
    write(fd, &clr, 1); tcdrain(fd); usleep(100000);
    uint8_t home = 0x0B;
    write(fd, &home, 1); tcdrain(fd); usleep(50000);
    write(fd, "Press buttons!", 14); tcdrain(fd);
    write(fd, "\n", 1); tcdrain(fd);
    write(fd, "Watching serial..", 17); tcdrain(fd);

    printf("=== Button Test ===\n");
    printf("Press front panel buttons. Watching /dev/cuau1 for data.\n");
    printf("Ctrl+C to stop.\n\n");

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    uint8_t buf[64];
    int count = 0;

    while (running) {
        int r = poll(&pfd, 1, 100);
        if (r > 0 && (pfd.revents & POLLIN)) {
            int n = read(fd, buf, sizeof(buf));
            if (n > 0) {
                count++;
                printf("[%3d] Received %d bytes:", count, n);
                for (int i = 0; i < n; i++)
                    printf(" 0x%02X", buf[i]);
                printf("  (");
                for (int i = 0; i < n; i++)
                    printf("%c", (buf[i] >= 0x20 && buf[i] < 0x7F) ? buf[i] : '.');
                printf(")\n");
                fflush(stdout);
            }
        }
    }

    /* Clear display */
    write(fd, &clr, 1); tcdrain(fd); usleep(50000);
    write(fd, &home, 1); tcdrain(fd); usleep(50000);
    write(fd, "Test complete.", 14); tcdrain(fd);

    printf("\nCaptured %d events.\n", count);
    close(fd);
    return 0;
}
