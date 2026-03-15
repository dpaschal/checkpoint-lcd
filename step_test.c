#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

static int lcd_fd = -1;
static int spk_fd = -1;

static void lcd_open(void) {
    lcd_fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    if (lcd_fd < 0) { perror("open cuau1"); return; }
    struct termios tio;
    tcgetattr(lcd_fd, &tio);
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 5;
    tcsetattr(lcd_fd, TCSANOW, &tio);
    tcflush(lcd_fd, TCIOFLUSH);
}

static void lcd_send(const void *data, int len) {
    write(lcd_fd, data, len);
    tcdrain(lcd_fd);
    usleep(50000);
}
static void lcd_sendb(uint8_t b) { lcd_send(&b, 1); }
static void lcd_send2(uint8_t a, uint8_t b) { uint8_t buf[2]={a,b}; lcd_send(buf,2); }

static void beep(void) {
    spk_fd = open("/dev/speaker", O_WRONLY);
    if (spk_fd >= 0) { write(spk_fd, "O2L8C", 5); close(spk_fd); }
}

static void wait_enter(const char *msg) {
    beep();
    printf(">> %s\n", msg);
    printf("   Press ENTER to continue...\n");
    fflush(stdout);
    getchar();
}

int main(int argc, char **argv) {
    int step = 1;
    if (argc > 1) step = atoi(argv[1]);

    lcd_open();
    if (lcd_fd < 0) return 1;

    switch (step) {
    case 1:
        printf("STEP 1: Init + Clear + 'Hello P-210!'\n");
        lcd_send2(0x1B, 0x40);  /* ESC @ = init */
        usleep(100000);
        lcd_sendb(0x0C);         /* clear */
        usleep(100000);
        lcd_sendb(0x0B);         /* home */
        usleep(50000);
        lcd_send("Hello P-210!", 12);
        wait_enter("Do you see 'Hello P-210!' on the LCD?");
        break;

    case 2:
        printf("STEP 2: Two lines of text\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        lcd_sendb(0x0B);
        usleep(50000);
        lcd_send("Line 1: Top Row", 15);
        lcd_send("\r\n", 2);
        lcd_send("Line 2: Bottom Row", 18);
        wait_enter("Do you see TWO lines of text?");
        break;

    case 3:
        printf("STEP 3: Newline with 0x0A only\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        lcd_sendb(0x0B);
        usleep(50000);
        lcd_send("Top via 0x0A", 12);
        lcd_sendb(0x0A);  /* linefeed */
        lcd_send("Bottom via 0x0A", 15);
        wait_enter("Do you see two lines? How does it look?");
        break;

    case 4:
        printf("STEP 4: Fill entire display\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        lcd_sendb(0x0B);
        usleep(50000);
        lcd_send("12345678901234567890", 20);
        lcd_sendb(0x0A);
        lcd_send("ABCDEFGHIJKLMNOPQRST", 20);
        wait_enter("Do you see 20 chars on each line? Numbers on top, letters on bottom?");
        break;

    case 5:
        printf("STEP 5: Clear screen only\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        usleep(100000);
        wait_enter("Is the screen cleared?");
        break;

    case 6:
        printf("STEP 6: Special chars and symbols\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        lcd_sendb(0x0B);
        usleep(50000);
        lcd_send("IP:192.168.1.1/24", 17);
        lcd_sendb(0x0A);
        lcd_send("Up:3d CPU:45%% 80F", 18);
        wait_enter("Can you read the IP address and stats?");
        break;

    case 7:
        printf("STEP 7: Read button response\n");
        lcd_send2(0x1B, 0x40);
        usleep(100000);
        lcd_sendb(0x0C);
        lcd_sendb(0x0B);
        usleep(50000);
        lcd_send("Press any button!", 17);
        printf("Waiting 5 seconds for button data from LCD...\n");
        uint8_t resp[64];
        for (int i = 0; i < 10; i++) {
            usleep(500000);
            int n = read(lcd_fd, resp, sizeof(resp));
            if (n > 0) {
                printf("  Received %d bytes:", n);
                for (int j = 0; j < n; j++) printf(" 0x%02X", resp[j]);
                printf("\n");
            }
        }
        wait_enter("Did pressing buttons produce any output above?");
        break;

    default:
        printf("Steps 1-7 available. Usage: ./step_test <step>\n");
        break;
    }

    close(lcd_fd);
    return 0;
}
