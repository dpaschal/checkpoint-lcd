/*
 * serial_test.c — Systematic EZIO-G500 serial protocol test
 * The cursor MOVED when we sent serial data — the LCD is on /dev/cuau1.
 * Try every combination methodically.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

static int lcd_fd;

static void lcd_open(speed_t baud) {
    if (lcd_fd >= 0) close(lcd_fd);
    lcd_fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    if (lcd_fd < 0) { perror("open cuau1"); return; }
    
    struct termios tio;
    tcgetattr(lcd_fd, &tio);
    cfmakeraw(&tio);
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);
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
static void lcd_send2(uint8_t a, uint8_t b) { uint8_t buf[2] = {a, b}; lcd_send(buf, 2); }

static int lcd_read(void *buf, int max, int timeout_ms) {
    usleep(timeout_ms * 1000);
    return read(lcd_fd, buf, max);
}

static void beep(void) {
    /* Use /dev/speaker on FreeBSD */
    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L4C", 5); close(sp); }
}

int main(void) {
    uint8_t resp[256];
    int n;

    printf("=== EZIO-G500 Serial LCD Test on /dev/cuau1 ===\n\n");

    /* Test 1: 115200 baud, EZIO text protocol */
    printf("--- Test 1: 115200 baud, ESC@ init + text ---\n");
    lcd_open(B115200);
    lcd_send2(0x1B, 0x40);      /* ESC @ = init */
    usleep(100000);
    lcd_sendb(0x0C);             /* clear screen (form feed) */
    usleep(100000);
    lcd_sendb(0x0B);             /* home (vertical tab) */
    usleep(50000);
    lcd_send("TEST 115200", 11);
    usleep(200000);
    n = lcd_read(resp, sizeof(resp), 200);
    printf("  Sent init+text. Read back %d bytes:", n);
    for (int i = 0; i < n; i++) printf(" %02X", resp[i]);
    printf("\n  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 2: 115200, just raw text with CR/LF */
    printf("--- Test 2: 115200, raw text with CR/LF ---\n");
    lcd_open(B115200);
    lcd_send("\r\n", 2);
    usleep(50000);
    lcd_send("HELLO RAW TEXT!\r\nLine 2 here.\r\n", 30);
    usleep(200000);
    printf("  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 3: 115200, EZIO graphics mode init + text */
    printf("--- Test 3: 115200, ESC G (graphics) + ESC T (text) ---\n");
    lcd_open(B115200);
    lcd_send2(0x1B, 0x40);      /* init */
    usleep(100000);
    lcd_sendb(0x0C);             /* clear */
    lcd_sendb(0x0B);             /* home */
    usleep(50000);
    /* Try text mode command if it exists */
    lcd_send2(0x1B, 0x54);      /* ESC T = maybe text mode? */
    usleep(50000);
    lcd_send("GRAPHICS TEST", 13);
    usleep(200000);
    printf("  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 4: 9600 baud */
    printf("--- Test 4: 9600 baud ---\n");
    lcd_open(B9600);
    lcd_send2(0x1B, 0x40);
    usleep(100000);
    lcd_sendb(0x0C);
    lcd_sendb(0x0B);
    lcd_send("TEST 9600", 9);
    usleep(200000);
    n = lcd_read(resp, sizeof(resp), 200);
    printf("  Read back %d bytes:", n);
    for (int i = 0; i < n; i++) printf(" %02X", resp[i]);
    printf("\n  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 5: 19200 baud */
    printf("--- Test 5: 19200 baud ---\n");
    lcd_open(B19200);
    lcd_send2(0x1B, 0x40);
    usleep(100000);
    lcd_sendb(0x0C);
    lcd_sendb(0x0B);
    lcd_send("TEST 19200", 10);
    usleep(200000);
    printf("  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 6: 38400 baud */
    printf("--- Test 6: 38400 baud ---\n");
    lcd_open(B38400);
    lcd_send2(0x1B, 0x40);
    usleep(100000);
    lcd_sendb(0x0C);
    lcd_sendb(0x0B);
    lcd_send("TEST 38400", 10);
    usleep(200000);
    printf("  >> Check LCD!\n");
    beep(); sleep(3);

    /* Test 7: 115200, try SDEC-specific: send 0xFF blocks */
    printf("--- Test 7: 115200, ESC@ + solid 0xFF blocks ---\n");
    lcd_open(B115200);
    lcd_send2(0x1B, 0x40);
    usleep(100000);
    lcd_sendb(0x0C);
    lcd_sendb(0x0B);
    usleep(50000);
    uint8_t blocks[20];
    memset(blocks, 0xFF, 20);
    lcd_send(blocks, 20);
    usleep(200000);
    printf("  >> Check LCD for solid blocks!\n");
    beep(); sleep(3);

    /* Test 8: 115200, no ESC init, just raw chars */
    printf("--- Test 8: 115200, NO init, just raw ASCII ---\n");
    lcd_open(B115200);
    lcd_send("ABCDEFGHIJKLMNOPQRST", 20);
    usleep(200000);
    printf("  >> Check LCD!\n");
    beep(); sleep(3);

    /* Double beep = done */
    beep(); usleep(300000); beep();
    printf("\n=== DONE — which test showed text? ===\n");

    if (lcd_fd >= 0) close(lcd_fd);
    return 0;
}
