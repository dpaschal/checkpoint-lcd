#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#ifdef __FreeBSD__
#include <machine/cpufunc.h>
#endif

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

    /* Enter SuperIO config mode at 0x2E (Winbond: write 0x87 twice) */
    outb(0x2E, 0x87);
    outb(0x2E, 0x87);

    /* Select LDN 1 (LPT) */
    outb(0x2E, 0x07);
    outb(0x2F, 0x01);

    /* Check current state */
    outb(0x2E, 0x30);
    uint8_t active = inb(0x2F);
    printf("LPT (LDN 1) active before: %d\n", active & 1);

    /* Verify base address */
    outb(0x2E, 0x60); uint8_t bhi = inb(0x2F);
    outb(0x2E, 0x61); uint8_t blo = inb(0x2F);
    printf("LPT base address: 0x%02X%02X\n", bhi, blo);

    /* Enable it! */
    outb(0x2E, 0x30);
    outb(0x2F, 0x01);

    /* Verify */
    outb(0x2E, 0x30);
    active = inb(0x2F);
    printf("LPT (LDN 1) active after: %d\n", active & 1);

    /* Exit config mode */
    outb(0x2E, 0xAA);

    /* Now test if 0x378 responds */
    usleep(10000);
    uint8_t d = inb(0x378);
    uint8_t s = inb(0x379);
    uint8_t c = inb(0x37A);
    printf("Port 0x378: data=0x%02X status=0x%02X ctrl=0x%02X\n", d, s, c);

    if (d == 0xFF && s == 0xFF && c == 0xFF)
        printf("Still reads FF — may need chipset LPC decode enable too\n");
    else
        printf("** LPT PORT IS ALIVE! **\n");

    /* Beep */
    outb(0x43, 0xB6);
    outb(0x42, 0xA9);
    outb(0x42, 0x04);
    uint8_t spk = inb(0x61);
    outb(0x61, spk | 0x03);
    usleep(500000);
    outb(0x61, spk & ~0x03);

#ifdef __FreeBSD__
    close(io_fd);
#endif
    return 0;
}
