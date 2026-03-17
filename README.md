# checkpoint-lcd

<p align="center">
  <a href="https://buymeacoffee.com/dpaschal">
    <img src="https://img.shields.io/badge/❤️🎆_THANKS!_🎆❤️-Support_This_Project-ff0000?style=for-the-badge" alt="Thanks!" height="40">
  </a>
</p>

<p align="center">
  <b>☕ Buy me Claude Code credits or support a project! ☕</b>
</p>
<p align="center">
  <i>Every donation keeps the code flowing — these tools are built with your support.</i>
</p>

<p align="center">
  <a href="https://buymeacoffee.com/dpaschal">
    <img src="https://cdn.buymeacoffee.com/buttons/v2/default-red.png" alt="Buy Me A Coffee" height="50">
  </a>
</p>

---

**LCD panel driver for Check Point P-210/12200 firewall appliances running OPNsense/FreeBSD.**

Reverse-engineered the Portwell EZIO-G500 front panel LCD on the Check Point P-210 (model 12200). This is the first open-source driver that actually controls the LCD on these appliances when running alternative firmware like OPNsense.

## The Problem

The Check Point P-210/12200 has a front panel LCD (128x64 pixel EZIO-G500) that works perfectly under Check Point's Gaia OS. When you install OPNsense, pfSense, or plain FreeBSD, the LCD just shows a blinking cursor — the built-in `sdeclcd` lcdproc driver doesn't work because:

1. **The SuperIO chip (Winbond @ 0x2E) has the LPT port disabled** — `sdeclcd` tries to use parallel port I/O at 0x378 but reads all 0xFF
2. **The LCD actually communicates over serial** (`/dev/cuau1`, COM2) using the EZIO-G500 text protocol, not the parallel port

Even enabling the LPT port in the SuperIO chip doesn't help — the LCD's text interface is serial.

## The Solution

`cpanel` talks directly to the LCD over `/dev/cuau1` at 115200 baud using the EZIO-G500 serial protocol:

```
ESC @ — Initialize/reset display
0x0C  — Clear screen
0x0B  — Cursor home
0x0A  — Newline
Raw ASCII text is displayed directly
```

### Display Specs

| Parameter | Value |
|-----------|-------|
| Display | Portwell EZIO-G500 (128x64 pixel, 8x8 font) |
| Text Mode | 16 columns x 8 rows |
| Interface | RS-232 serial, 115200 8N1 |
| Device | `/dev/cuau1` (FreeBSD) or `/dev/ttyS1` (Linux) |
| Buttons | 5+ front panel buttons (via serial or LPT status port) |

## Building

```bash
# On the P-210 (FreeBSD/OPNsense):
make
./cpanel demo

# On Linux (for development):
make
```

No dependencies. Pure C11. ~500 lines total.

## Usage

```bash
# Initialize and clear
./cpanel init

# Write text to specific rows
./cpanel write "Line 1 text" "Line 2 text" "Line 3" ...

# Write to a specific row (0-7)
./cpanel text 0 "Hello P-210!"

# System status display
./cpanel status

# Live updating monitor (Ctrl+C to stop)
./cpanel monitor

# Full-screen clock
./cpanel clock

# Animated demo
./cpanel demo

# Use a different serial device
./cpanel -d /dev/ttyS1 demo
```

## Hardware

The Check Point P-210 (model 12200) is an x86 firewall appliance:

- Intel Core i5-750 (4 cores, 2.67 GHz)
- 8-9 GB DDR3 RAM
- 8x Intel 82574L Gigabit Ethernet
- 2x Samsung 480GB SSD (SATA)
- Front panel: EZIO-G500 LCD + buttons + LEDs
- Available on eBay for ~$50-100

## Reverse Engineering Story

This driver was built in a single session by connecting a laptop to the P-210's serial console and systematically figuring out how the LCD works:

1. **Disassembled OPNsense's `sdeclcd.so`** — found it uses parallel port I/O at 0x378, but the port reads all 0xFF
2. **Scanned all I/O ports** (0x000-0xFFF) — discovered a Winbond SuperIO chip at 0x2E with the LPT disabled (LDN 1, active=0)
3. **Enabled the LPT port** via SuperIO config — port came alive but LCD still didn't respond to HD44780 commands
4. **Noticed the cursor moved** when sending data to `/dev/cuau1` (COM2) — the LCD is on the serial port, not parallel
5. **Tested the EZIO-G500 serial protocol** — ESC @ init + raw ASCII text = working display
6. **Mapped the display geometry** — 16 columns x 8 rows (128x64 pixels with 8x8 font)

The `diag.c`, `portscan.c`, `findlpt.c`, and `serial_test.c` files are the actual tools used during this reverse engineering process.

## Related Work

- [tchatzi/EZIO-G500](https://github.com/tchatzi/EZIO-G500) — Perl scripts for the EZIO-G500 (graphical mode, incomplete)
- [Saint-Frater's RE](https://git.nox-rhea.org/globals/reverse-engineering/ezio-g500) — EZIO-G500 command set documentation
- [Netgate Forum: CheckPoint LCD](https://forum.netgate.com/topic/155804/operate-checkpoint-4800-lcd-screen-with-pfsense-ezio-g500) — Community attempts (unfinished)
- [LCDproc Issue #159](https://github.com/lcdproc/lcdproc/issues/159) — Open request for EZIO-G500 support

## License

MIT

## Contributing

PRs welcome. Key areas:
- Button input handling (protocol TBD)
- LED control (3 bi-color LEDs on the front panel)
- Graphics mode (the EZIO-G500 supports 128x64 pixel bitmap rendering)
- OPNsense plugin integration
- lcdproc driver
