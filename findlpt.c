/*
 * findlpt.c — Find the LPT port on a Check Point P-210
 * Probes SuperIO chips and PCH LPC config to find disabled LPT
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

/* SuperIO chip probing — try common config ports */
void probe_superio(uint16_t cfg_port)
{
    uint16_t idx = cfg_port;
    uint16_t dat = cfg_port + 1;

    /* Enter config mode — try different sequences */
    /* Winbond/Nuvoton: write 0x87 twice */
    outb(idx, 0x87); outb(idx, 0x87);
    
    /* Read chip ID */
    outb(idx, 0x20); uint8_t id_hi = inb(dat);
    outb(idx, 0x21); uint8_t id_lo = inb(dat);
    printf("  SuperIO @ 0x%03X: ID = 0x%02X%02X", cfg_port, id_hi, id_lo);
    
    if (id_hi == 0xFF && id_lo == 0xFF) {
        printf(" (not present)\n");
        /* Exit config mode */
        outb(idx, 0xAA);
        return;
    }
    printf(" ** FOUND **\n");
    
    /* Try to find LPT logical device */
    /* Common LPT LDN: 1 (Winbond), 3 (ITE), 0x01 (SMSC) */
    for (int ldn = 0; ldn < 16; ldn++) {
        outb(idx, 0x07); outb(dat, ldn);  /* select LDN */
        outb(idx, 0x60); uint8_t base_hi = inb(dat);
        outb(idx, 0x61); uint8_t base_lo = inb(dat);
        outb(idx, 0x30); uint8_t active = inb(dat);
        uint16_t base = (base_hi << 8) | base_lo;
        if (base != 0 && base != 0xFFFF) {
            printf("    LDN %2d: base=0x%04X active=%d\n", ldn, base, active & 1);
        }
    }
    
    /* Exit config mode */
    outb(idx, 0xAA);
}

void probe_superio_ite(uint16_t cfg_port)
{
    uint16_t idx = cfg_port;
    uint16_t dat = cfg_port + 1;
    
    /* ITE enter sequence: write 0x87 0x01 0x55 0x55 (or 0xAA) */
    outb(idx, 0x87); outb(idx, 0x01); outb(idx, 0x55); outb(idx, 0x55);
    
    outb(idx, 0x20); uint8_t id_hi = inb(dat);
    outb(idx, 0x21); uint8_t id_lo = inb(dat);
    printf("  ITE SuperIO @ 0x%03X: ID = 0x%02X%02X", cfg_port, id_hi, id_lo);
    if (id_hi != 0xFF) printf(" ** FOUND **\n"); else printf(" (not present)\n");
    
    outb(idx, 0x02); outb(dat, 0x02); /* exit */
}

int main(void) {
#ifdef __FreeBSD__
    int io_fd = open("/dev/io", O_RDWR);
    if (io_fd < 0) { perror("open /dev/io"); return 1; }
#else
    if (ioperm(0x2E, 2, 1) != 0 || ioperm(0x4E, 2, 1) != 0 ||
        ioperm(0x61, 1, 1) != 0 || ioperm(0x100, 0xF00, 1) != 0) {
        perror("ioperm"); return 1;
    }
#endif

    printf("=== Probing SuperIO chips ===\n");
    probe_superio(0x2E);
    probe_superio(0x4E);
    probe_superio_ite(0x2E);
    probe_superio_ite(0x4E);
    
    /* Also try SMSC enter sequence */
    printf("\n=== Checking LPC/ISA decode ranges via PCI ===\n");
    /* On Intel PCH, PCI device 31:0 (LPC bridge) has LPT decode registers */
    /* Read PCI config space for bus 0, device 31, function 0 */
    /* PCI config address: 0x80000000 | (bus<<16) | (dev<<11) | (func<<8) | reg */
    uint32_t lpc_addr = 0x80000000 | (0 << 16) | (31 << 11) | (0 << 8);
    
    /* Register 0x80: I/O decode ranges (LPC_IO_DEC) */
    outb(0xCF8, (lpc_addr | 0x80) & 0xFF);
    outb(0xCF9, ((lpc_addr | 0x80) >> 8) & 0xFF);
    outb(0xCFA, ((lpc_addr | 0x80) >> 16) & 0xFF);
    outb(0xCFB, ((lpc_addr | 0x80) >> 24) & 0xFF);
    
    /* Actually, PCI config access needs 32-bit I/O which is tricky from C.
     * Let's use a different approach - read from sysctl or pciconf */
    printf("  (Use pciconf -lv or devinfo -v to check LPC decode)\n");
    
    /* Brute force: scan ALL I/O ports 0x000-0xFFF for non-0xFF */
    printf("\n=== Full I/O port scan (0x000-0xFFF) for non-FF ===\n");
    printf("  (Skipping known ranges: UART0, UART1, VGA, KBD)\n");
    for (uint16_t p = 0x100; p < 0x1000; p++) {
        /* Skip known ranges */
        if (p >= 0x2F8 && p <= 0x2FF) continue; /* UART1 */
        if (p >= 0x3C0 && p <= 0x3DF) continue; /* VGA */
        if (p >= 0x3F8 && p <= 0x3FF) continue; /* UART0 */
        if (p >= 0x60 && p <= 0x64) continue;   /* KBD */
        
        uint8_t val = inb(p);
        if (val != 0xFF) {
            printf("  0x%03X = 0x%02X\n", p, val);
        }
    }

    /* Beep to signal done */
    outb(0x43, 0xB6);
    outb(0x42, 0xA9);
    outb(0x42, 0x04);
    uint8_t spk = inb(0x61);
    outb(0x61, spk | 0x03);
    usleep(200000);
    outb(0x61, spk & ~0x03);
    
    printf("\nDone! Beeped.\n");

#ifdef __FreeBSD__
    close(io_fd);
#endif
    return 0;
}
