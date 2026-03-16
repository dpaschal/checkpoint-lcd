/*
 * btn_test2.c — Test buttons on BOTH serial and LPT status port
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <signal.h>
#include <poll.h>

#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#endif

static volatile sig_atomic_t running = 1;
static void sighandler(int s) { (void)s; running = 0; }

int main(void) {
    signal(SIGINT, sighandler);

    /* Open serial */
    int sfd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    if (sfd < 0) { perror("open cuau1"); return 1; }
    struct termios tio;
    tcgetattr(sfd, &tio);
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    tcsetattr(sfd, TCSANOW, &tio);
    tcflush(sfd, TCIOFLUSH);

    /* Enable LPT via SuperIO */
#ifdef __FreeBSD__
    int iofd = open("/dev/io", O_RDWR);
    if (iofd >= 0) {
        outb(0x2E, 0x87); outb(0x2E, 0x87);
        outb(0x2E, 0x07); outb(0x2F, 0x01);
        outb(0x2E, 0x30); outb(0x2F, 0x01);
        outb(0x2E, 0xAA);
        usleep(10000);
    }
#endif

    /* Show prompt on LCD */
    uint8_t cmd[] = {0x1B, 0x40};
    write(sfd, cmd, 2); tcdrain(sfd); usleep(100000);
    uint8_t clr = 0x0C;
    write(sfd, &clr, 1); tcdrain(sfd); usleep(100000);
    uint8_t hm = 0x0B;
    write(sfd, &hm, 1); tcdrain(sfd); usleep(50000);
    write(sfd, "Press buttons!", 14); tcdrain(sfd);
    write(sfd, "\n", 1); tcdrain(sfd);
    write(sfd, "Testing 20 sec..", 16); tcdrain(sfd);

    printf("=== Button Test (serial + LPT) ===\n");
    printf("Press front panel buttons for 20 seconds.\n\n");

    struct pollfd pfd = { .fd = sfd, .events = POLLIN };
    uint8_t buf[64];
    uint8_t last_lpt = 0xFF;
    int serial_count = 0;
    int lpt_count = 0;

    for (int tick = 0; tick < 200 && running; tick++) {
        /* Check serial */
        int r = poll(&pfd, 1, 100);
        if (r > 0 && (pfd.revents & POLLIN)) {
            int n = read(sfd, buf, sizeof(buf));
            if (n > 0) {
                serial_count++;
                printf("[SERIAL %3d] %d bytes:", serial_count, n);
                for (int i = 0; i < n; i++)
                    printf(" 0x%02X", buf[i]);
                printf("\n");
                fflush(stdout);
            }
        }

        /* Check LPT status port */
#ifdef __FreeBSD__
        if (iofd >= 0) {
            uint8_t lpt = inb(0x379);
            if (lpt != last_lpt) {
                lpt_count++;
                printf("[LPT    %3d] 0x%02X -> 0x%02X (changed bits: 0x%02X)\n",
                       lpt_count, last_lpt, lpt, last_lpt ^ lpt);
                fflush(stdout);
                last_lpt = lpt;
            }
        }
#endif
    }

    printf("\nSerial events: %d\n", serial_count);
    printf("LPT events: %d\n", lpt_count);

#ifdef __FreeBSD__
    if (iofd >= 0) close(iofd);
#endif
    close(sfd);
    return 0;
}
