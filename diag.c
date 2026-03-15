/*
 * diag.c — SDEC LCD diagnostic: brute-force strobe pattern finder
 *
 * Tries multiple E/RS bit mappings and polarities to find what works.
 * Run on the P-210 after killing LCDd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <time.h>

#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#else
#include <sys/io.h>
#endif

#define DATA  0x378
#define STAT  0x379
#define CTRL  0x37a

static inline void pout(uint16_t port, uint8_t val) {
#ifdef __FreeBSD__
    outb(port, val);
#else
    outb(val, port);
#endif
}

static void usleep_wrap(int us) { usleep(us); }

/*
 * Try to init + write text with a given strobe function.
 * The strobe function takes (rs, data_byte) where rs=0 for command, rs=1 for data.
 */

/* Method 1: E=bit1, transitions 0→1 (original interpretation) */
static void strobe_m1(int rs, uint8_t val) {
    uint8_t c1 = rs ? 0x00 : 0x08;  /* bit3=RS inverted: 1=cmd(RS LOW) */
    uint8_t c2 = c1 | 0x02;          /* set bit1 */
    pout(CTRL, c1);
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, c2);
    usleep_wrap(5);
}

/* Method 2: E=bit1, transitions 1→0 (reversed) */
static void strobe_m2(int rs, uint8_t val) {
    uint8_t c1 = (rs ? 0x00 : 0x08) | 0x02;  /* bit1 starts HIGH */
    uint8_t c2 = rs ? 0x00 : 0x08;             /* bit1 goes LOW */
    pout(CTRL, c1);
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, c2);
    usleep_wrap(5);
}

/* Method 3: E=bit1, full pulse LOW→HIGH→LOW */
static void strobe_m3(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x08;
    pout(CTRL, base);         /* E LOW */
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base | 0x02);  /* E HIGH */
    usleep_wrap(5);
    pout(CTRL, base);         /* E LOW (falling edge) */
    usleep_wrap(5);
}

/* Method 4: E=bit1, full pulse HIGH→LOW→HIGH (inverted sense) */
static void strobe_m4(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x08;
    pout(CTRL, base | 0x02);  /* E HIGH */
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base);         /* E LOW */
    usleep_wrap(5);
    pout(CTRL, base | 0x02);  /* E HIGH */
    usleep_wrap(5);
}

/* Method 5: E=bit2 (Init, not inverted), pulse HIGH→LOW */
static void strobe_m5(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x08;
    pout(CTRL, base | 0x04);  /* E HIGH (bit2 not inverted) */
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base);         /* E LOW (falling edge) */
    usleep_wrap(5);
}

/* Method 6: E=bit2, pulse LOW→HIGH→LOW */
static void strobe_m6(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x08;
    pout(CTRL, base);
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base | 0x04);
    usleep_wrap(5);
    pout(CTRL, base);
    usleep_wrap(5);
}

/* Method 7: Exact disassembly replay - cmd uses bl^0x09 then XOR 0x0B */
static void strobe_m7(int rs, uint8_t val) {
    uint8_t bl = 1; /* backlight on */
    uint8_t c;
    if (rs == 0) {
        /* Command: exact disassembly pattern */
        c = bl ^ 0x09;  /* 0x08 */
        pout(CTRL, c);
        pout(DATA, val);
        usleep_wrap(50);
        c ^= 0x0B;      /* 0x08 ^ 0x0B = 0x03 */
        pout(CTRL, c);
        usleep_wrap(50);
    } else {
        /* Data: exact disassembly pattern */
        c = !bl;         /* 0x00 */
        pout(CTRL, c);
        pout(DATA, val);
        usleep_wrap(50);
        c = bl ^ 0x03;   /* 0x02 */
        pout(CTRL, c);
        usleep_wrap(50);
    }
}

/* Method 8: Same as m7 but with MUCH longer delays */
static void strobe_m8(int rs, uint8_t val) {
    uint8_t bl = 1;
    uint8_t c;
    if (rs == 0) {
        c = bl ^ 0x09;
        pout(CTRL, c);
        usleep_wrap(100);
        pout(DATA, val);
        usleep_wrap(500);
        c ^= 0x0B;
        pout(CTRL, c);
        usleep_wrap(500);
    } else {
        c = !bl;
        pout(CTRL, c);
        usleep_wrap(100);
        pout(DATA, val);
        usleep_wrap(500);
        c = bl ^ 0x03;
        pout(CTRL, c);
        usleep_wrap(500);
    }
}

/* Method 9: RS on bit0 instead of bit3 (maybe wiring is different) */
static void strobe_m9(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x01;  /* RS on bit0 */
    pout(CTRL, base);
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base | 0x02);  /* E on bit1 */
    usleep_wrap(5);
    pout(CTRL, base);
    usleep_wrap(5);
}

/* Method 10: RS=bit0(inverted), E=bit2(not inverted) */
static void strobe_m10(int rs, uint8_t val) {
    uint8_t base = rs ? 0x00 : 0x01;
    pout(CTRL, base | 0x04);
    pout(DATA, val);
    usleep_wrap(5);
    pout(CTRL, base);
    usleep_wrap(5);
}

typedef void (*strobe_fn)(int rs, uint8_t val);

static void try_method(const char *name, strobe_fn fn)
{
    printf("  Testing method: %s\n", name);

    /* HD44780 init */
    usleep_wrap(20000);
    fn(0, 0x38);    /* function set */
    usleep_wrap(5000);
    fn(0, 0x38);
    usleep_wrap(200);
    fn(0, 0x38);
    fn(0, 0x0C);    /* display on, cursor off */
    fn(0, 0x01);    /* clear */
    usleep_wrap(2000);
    fn(0, 0x06);    /* entry mode */

    /* Write test text */
    fn(0, 0x80);    /* DDRAM address line 1 */
    const char *msg = "Hello P-210!";
    for (const char *p = msg; *p; p++)
        fn(1, *p);

    fn(0, 0xC0);    /* DDRAM address line 2 */
    const char *msg2 = name;
    for (const char *p = msg2; *p; p++)
        fn(1, *p);

    printf("    >> Check LCD now! (3 sec pause)\n");
    fflush(stdout);
    sleep(3);
}

int main(void)
{
#ifdef __FreeBSD__
    int io_fd = open("/dev/io", O_RDWR);
    if (io_fd < 0) { perror("open /dev/io"); return 1; }
#else
    if (ioperm(DATA, 3, 1) != 0) { perror("ioperm"); return 1; }
#endif

    printf("SDEC LCD Diagnostic — Trying all strobe patterns\n");
    printf("Watch the LCD! Each method pauses 3 seconds.\n\n");

    /* First, read current status port for debug */
    uint8_t stat = inb(STAT);
    printf("Status port (0x379): 0x%02X\n", stat);
    uint8_t ctrl = inb(CTRL);
    printf("Control port (0x37a): 0x%02X\n\n", ctrl);

    struct { const char *name; strobe_fn fn; } methods[] = {
        {"M1: E=bit1 0->1",           strobe_m1},
        {"M2: E=bit1 1->0",           strobe_m2},
        {"M3: E=bit1 pulse L-H-L",    strobe_m3},
        {"M4: E=bit1 pulse H-L-H",    strobe_m4},
        {"M5: E=bit2 H->L",           strobe_m5},
        {"M6: E=bit2 pulse L-H-L",    strobe_m6},
        {"M7: Exact disasm pattern",   strobe_m7},
        {"M8: Disasm + long delays",   strobe_m8},
        {"M9: RS=bit0 E=bit1 pulse",   strobe_m9},
        {"M10: RS=bit0 E=bit2",        strobe_m10},
    };
    int n = sizeof(methods) / sizeof(methods[0]);

    for (int i = 0; i < n; i++) {
        try_method(methods[i].name, methods[i].fn);
    }

    printf("\nDone! Which method showed text?\n");

#ifdef __FreeBSD__
    close(io_fd);
#endif
    return 0;
}
