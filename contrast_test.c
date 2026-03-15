/*
 * contrast_test.c — Fill LCD with solid blocks to test contrast
 * Also tries different character patterns to find visible ones.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#endif

#define DATA 0x378
#define STAT 0x379
#define CTRL 0x37a

/* Exact strobe from sdeclcd.so disassembly, backlight=1 (bl=1) */
static void lcd_cmd(uint8_t cmd) {
    /* Phase 1: E HIGH, RS LOW — bl(1) ^ 0x09 = 0x08 */
    outb(CTRL, 0x08);
    outb(DATA, cmd);
    usleep(100);
    /* Phase 2: E toggle — 0x08 ^ 0x0B = 0x03 */
    outb(CTRL, 0x03);
    usleep(100);
    if (cmd <= 0x03) usleep(3000);
}

static void lcd_data(uint8_t d) {
    /* Phase 1: E HIGH, RS HIGH — !bl = 0x00 */
    outb(CTRL, 0x00);
    outb(DATA, d);
    usleep(100);
    /* Phase 2: E toggle — bl(1) ^ 0x03 = 0x02 */
    outb(CTRL, 0x02);
    usleep(100);
}

/* Try REVERSED E polarity */
static void lcd_cmd_rev(uint8_t cmd) {
    outb(CTRL, 0x03);   /* start with bit1 set */
    outb(DATA, cmd);
    usleep(100);
    outb(CTRL, 0x08);   /* drop bit1, set bit3 */
    usleep(100);
    if (cmd <= 0x03) usleep(3000);
}

static void lcd_data_rev(uint8_t d) {
    outb(CTRL, 0x02);   /* start with bit1 set */
    outb(DATA, d);
    usleep(100);
    outb(CTRL, 0x00);   /* drop bit1 */
    usleep(100);
}

/* 3-phase pulse: E LOW → HIGH → LOW */
static void lcd_cmd_pulse(uint8_t cmd) {
    outb(DATA, cmd);
    outb(CTRL, 0x08);   /* RS=cmd, E=LOW (bit1=0) */
    usleep(50);
    outb(CTRL, 0x0A);   /* RS=cmd, E=HIGH (bit1=1) */
    usleep(50);
    outb(CTRL, 0x08);   /* RS=cmd, E=LOW — falling edge */
    usleep(50);
    if (cmd <= 0x03) usleep(3000);
}

static void lcd_data_pulse(uint8_t d) {
    outb(DATA, d);
    outb(CTRL, 0x00);   /* RS=data, E=LOW */
    usleep(50);
    outb(CTRL, 0x02);   /* RS=data, E=HIGH */
    usleep(50);
    outb(CTRL, 0x00);   /* RS=data, E=LOW — falling edge */
    usleep(50);
}

typedef void (*cmd_fn)(uint8_t);
typedef void (*data_fn)(uint8_t);

static void init_and_fill(const char *name, cmd_fn cf, data_fn df) {
    printf("  %s: init...", name);
    usleep(50000);
    cf(0x38); usleep(5000);
    cf(0x38); usleep(200);
    cf(0x38);
    cf(0x0F);  /* display ON, cursor ON, blink ON — maximum visibility */
    cf(0x01);  /* clear */
    usleep(3000);
    cf(0x06);  /* entry mode */
    
    /* Fill with solid blocks (0xFF) */
    cf(0x80);  /* row 0 */
    for (int i = 0; i < 20; i++) df(0xFF);
    cf(0xC0);  /* row 1 */
    for (int i = 0; i < 20; i++) df(0xFF);
    printf(" blocks...");
    
    sleep(2);
    
    /* Now try visible ASCII */
    cf(0x01); usleep(3000);
    cf(0x80);
    const char *t1 = "HELLO P-210 LCD!";
    for (const char *p = t1; *p; p++) df(*p);
    cf(0xC0);
    const char *t2 = "Can you see this?";
    for (const char *p = t2; *p; p++) df(*p);
    printf(" text. Check!\n");
    
    sleep(3);
}

int main(void) {
#ifdef __FreeBSD__
    int io_fd = open("/dev/io", O_RDWR);
    if (io_fd < 0) { perror("open /dev/io"); return 1; }
#else
    if (ioperm(0x2E, 2, 1) != 0 || ioperm(0x43, 1, 1) != 0 ||
        ioperm(0x42, 1, 1) != 0 || ioperm(0x61, 1, 1) != 0 ||
        ioperm(0x378, 3, 1) != 0) {
        perror("ioperm"); return 1;
    }
#endif

    /* Enable LPT first */
    outb(0x2E, 0x87); outb(0x2E, 0x87);
    outb(0x2E, 0x07); outb(0x2F, 0x01);
    outb(0x2E, 0x30); outb(0x2F, 0x01);
    outb(0x2E, 0xAA);
    usleep(10000);

    printf("LPT status: data=0x%02X status=0x%02X ctrl=0x%02X\n\n",
           inb(DATA), inb(STAT), inb(CTRL));

    printf("=== Testing strobe patterns (LPT enabled) ===\n");
    printf("Each pattern: solid blocks (2s) then text (3s)\n\n");

    init_and_fill("Original", lcd_cmd, lcd_data);
    init_and_fill("Reversed E", lcd_cmd_rev, lcd_data_rev);
    init_and_fill("3-phase pulse", lcd_cmd_pulse, lcd_data_pulse);

    /* Beep when done */
    outb(0x43, 0xB6); outb(0x42, 0xA9); outb(0x42, 0x04);
    uint8_t spk = inb(0x61);
    outb(0x61, spk | 0x03); usleep(200000); outb(0x61, spk & ~0x03);
    usleep(200000);
    outb(0x61, spk | 0x03); usleep(200000); outb(0x61, spk & ~0x03);

    printf("\nDone! Two beeps.\n");

#ifdef __FreeBSD__
    close(io_fd);
#endif
    return 0;
}
