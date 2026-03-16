#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>

/* 5x8 font - subset for demo */
static const uint8_t font[][5] = {
    [' '-' '] = {0x00,0x00,0x00,0x00,0x00},
    ['!'-' '] = {0x00,0x00,0x5F,0x00,0x00},
    ['0'-' '] = {0x3E,0x51,0x49,0x45,0x3E},
    ['1'-' '] = {0x00,0x42,0x7F,0x40,0x00},
    ['2'-' '] = {0x42,0x61,0x51,0x49,0x46},
    ['3'-' '] = {0x21,0x41,0x45,0x4B,0x31},
    ['4'-' '] = {0x18,0x14,0x12,0x7F,0x10},
    ['5'-' '] = {0x27,0x45,0x45,0x45,0x39},
    ['6'-' '] = {0x3C,0x4A,0x49,0x49,0x30},
    ['7'-' '] = {0x01,0x71,0x09,0x05,0x03},
    ['8'-' '] = {0x36,0x49,0x49,0x49,0x36},
    ['9'-' '] = {0x06,0x49,0x49,0x29,0x1E},
    [':'-' '] = {0x00,0x36,0x36,0x00,0x00},
    ['A'-' '] = {0x7E,0x11,0x11,0x11,0x7E},
    ['B'-' '] = {0x7F,0x49,0x49,0x49,0x36},
    ['C'-' '] = {0x3E,0x41,0x41,0x41,0x22},
    ['D'-' '] = {0x7F,0x41,0x41,0x22,0x1C},
    ['E'-' '] = {0x7F,0x49,0x49,0x49,0x41},
    ['F'-' '] = {0x7F,0x09,0x09,0x09,0x01},
    ['G'-' '] = {0x3E,0x41,0x49,0x49,0x7A},
    ['H'-' '] = {0x7F,0x08,0x08,0x08,0x7F},
    ['I'-' '] = {0x00,0x41,0x7F,0x41,0x00},
    ['K'-' '] = {0x7F,0x08,0x14,0x22,0x41},
    ['L'-' '] = {0x7F,0x40,0x40,0x40,0x40},
    ['M'-' '] = {0x7F,0x02,0x0C,0x02,0x7F},
    ['N'-' '] = {0x7F,0x04,0x08,0x10,0x7F},
    ['O'-' '] = {0x3E,0x41,0x41,0x41,0x3E},
    ['P'-' '] = {0x7F,0x09,0x09,0x09,0x06},
    ['R'-' '] = {0x7F,0x09,0x19,0x29,0x46},
    ['S'-' '] = {0x46,0x49,0x49,0x49,0x31},
    ['T'-' '] = {0x01,0x01,0x7F,0x01,0x01},
    ['U'-' '] = {0x3F,0x40,0x40,0x40,0x3F},
    ['V'-' '] = {0x1F,0x20,0x40,0x20,0x1F},
    ['W'-' '] = {0x3F,0x40,0x38,0x40,0x3F},
    ['X'-' '] = {0x63,0x14,0x08,0x14,0x63},
    ['Z'-' '] = {0x61,0x51,0x49,0x45,0x43},
    ['-'-' '] = {0x08,0x08,0x08,0x08,0x08},
    ['.'-' '] = {0x00,0x60,0x60,0x00,0x00},
    ['/'-' '] = {0x20,0x10,0x08,0x04,0x02},
    ['%'-' '] = {0x23,0x13,0x08,0x64,0x62},
};

/* 128x64 framebuffer — each byte = 8 vertical pixels, LSB = top */
static uint8_t fb[1024];

static void fb_clear(int inverted) {
    memset(fb, inverted ? 0xFF : 0x00, 1024);
}

static void fb_char(int cx, int cy, char ch, int inverted) {
    if (ch < ' ' || ch > 'Z') ch = ' ';
    const uint8_t *g = font[ch - ' '];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            int x = cx + col;
            int y = cy + row;
            if (x < 0 || x >= 128 || y < 0 || y >= 64) continue;
            int panel = x / 64;
            int pcol = x % 64;
            int page = y / 8;
            int bit = y % 8;
            int idx = panel * 512 + page * 64 + pcol;
            int pixel_on = (g[col] >> row) & 1;
            if (inverted) pixel_on = !pixel_on;
            if (pixel_on)
                fb[idx] |= (1 << bit);
            else
                fb[idx] &= ~(1 << bit);
        }
    }
}

static void fb_text(int x, int y, const char *text, int inverted) {
    for (const char *p = text; *p; p++) {
        fb_char(x, y, *p, inverted);
        x += 6;
    }
}

int main(int argc, char **argv) {
    int inverted = (argc > 1 && strcmp(argv[1], "inv") == 0);

    int fd = open("/dev/cuau1", O_RDWR | O_NOCTTY);
    struct termios tio;
    tcgetattr(fd, &tio); cfmakeraw(&tio);
    cfsetispeed(&tio, B115200); cfsetospeed(&tio, B115200);
    tio.c_cflag |= (CLOCAL | CREAD | CS8);
    tio.c_cflag &= ~(PARENB | CSTOPB | CRTSCTS);
    tio.c_cc[VMIN]=0; tio.c_cc[VTIME]=1;
    tcsetattr(fd, TCSANOW, &tio);
    tcflush(fd, TCIOFLUSH);

    /* Init */
    uint8_t init[] = {0x1B, 0x40};
    write(fd, init, 2); tcdrain(fd); usleep(100000);

    /* Build framebuffer */
    fb_clear(inverted);
    fb_text(4, 4,  "CHECK POINT", inverted);
    fb_text(16, 14, "P-210", inverted);
    fb_text(4, 28, "INVERTED MODE", inverted);
    fb_text(4, 40, "DARK ON LIGHT", inverted);
    fb_text(4, 52, "128X64 GFX!", inverted);

    /* XOR invert for display (display expects 0=on, 1=off) */
    for (int i = 0; i < 1024; i++)
        fb[i] ^= 0xFF;

    /* Enter graphics mode and send framebuffer */
    uint8_t gfx[] = {0x1B, 0x47};
    write(fd, gfx, 2); tcdrain(fd); usleep(50000);

    /* Send in interleaved order: left page, right page alternating */
    for (int page = 0; page < 8; page++) {
        write(fd, &fb[page * 64], 64);         /* left panel */
        tcdrain(fd); usleep(3000);
        write(fd, &fb[512 + page * 64], 64);   /* right panel */
        tcdrain(fd); usleep(3000);
    }

    int sp = open("/dev/speaker", O_WRONLY);
    if (sp >= 0) { write(sp, "O2L8C", 5); close(sp); }

    printf("Sent %s graphics\n", inverted ? "INVERTED" : "normal");
    close(fd);
    return 0;
}
