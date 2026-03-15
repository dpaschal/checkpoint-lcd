/*
 * portscan.c — Find the actual I/O port for the LCD
 * Scans common LPT addresses and checks for non-0xFF response.
 * Also tries the PC speaker beep.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#else
#include <sys/io.h>
#endif

int main(void) {
#ifdef __FreeBSD__
    int io_fd = open("/dev/io", O_RDWR);
    if (io_fd < 0) { perror("open /dev/io"); return 1; }
#else
    /* WARNING: Grants access to a wide I/O range for diagnostic scanning.
     * Only run on hardware you own. Do not use in production. */
    if (ioperm(0x2E, 2, 1) != 0 || ioperm(0x42, 2, 1) != 0 ||
        ioperm(0x60, 16, 1) != 0 || ioperm(0x61, 1, 1) != 0 ||
        ioperm(0x200, 0xE00, 1) != 0) {
        perror("ioperm"); return 1;
    }
#endif

    /* Beep using PC speaker - OPNsense uses this */
    printf("=== Trying PC speaker beep ===\n");
    /* Program PIT channel 2 for ~1000 Hz */
    outb(0x43, 0xB6);           /* channel 2, mode 3, binary */
    outb(0x42, 0xA9);           /* low byte of 1193 (1000 Hz) */
    outb(0x42, 0x04);           /* high byte */
    uint8_t spk = inb(0x61);
    outb(0x61, spk | 0x03);     /* enable speaker */
    usleep(300000);              /* beep for 300ms */
    outb(0x61, spk & ~0x03);    /* disable speaker */
    printf("Beep sent!\n\n");

    /* Scan for LPT/LCD ports */
    printf("=== Scanning I/O ports ===\n");
    
    /* Common LPT base addresses */
    uint16_t lpt_bases[] = { 0x378, 0x278, 0x3BC, 0 };
    for (int i = 0; lpt_bases[i]; i++) {
        uint16_t base = lpt_bases[i];
        uint8_t d = inb(base);
        uint8_t s = inb(base + 1);
        uint8_t c = inb(base + 2);
        printf("Port 0x%03X: data=0x%02X status=0x%02X ctrl=0x%02X %s\n",
               base, d, s, c,
               (d == 0xFF && s == 0xFF && c == 0xFF) ? "(empty)" : "** FOUND **");
    }

    /* Scan wider range for any responsive ports */
    printf("\n=== Scanning 0x200-0x400 for non-FF ports ===\n");
    int found = 0;
    for (uint16_t port = 0x200; port < 0x400; port++) {
        uint8_t val = inb(port);
        if (val != 0xFF) {
            printf("  0x%03X = 0x%02X\n", port, val);
            found++;
            if (found > 50) { printf("  (truncated)\n"); break; }
        }
    }
    if (!found) printf("  Nothing found in 0x200-0x400 range!\n");

    /* Also scan SuperIO / ISA ranges */
    printf("\n=== Scanning 0x2E-0x2F (SuperIO) ===\n");
    printf("  0x2E = 0x%02X\n", inb(0x2E));
    printf("  0x2F = 0x%02X\n", inb(0x2F));

    /* Check if LPT is maybe memory-mapped or behind a different controller */
    printf("\n=== Scanning 0x60-0x6F (keyboard/misc) ===\n");
    for (uint16_t p = 0x60; p <= 0x6F; p++)
        printf("  0x%02X = 0x%02X\n", p, inb(p));

#ifdef __FreeBSD__
    close(io_fd);
#endif
    return 0;
}
