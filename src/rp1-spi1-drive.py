#!/usr/bin/env python3
"""
Set RP1 SPI1 header pins (GPIO16/19/20/21) to 12mA drive + fast slew.

Root cause: RP1 GPIO defaults to 4mA drive strength. BCM2711 (Pi 4) uses
16mA. The Navio2 RCIO STM32F103 needs sufficient drive to receive clean
SPI signals through the HAT PCB traces. 12mA is the closest RP1 can get.

Run before RCIO kernel modules load (via rp1-spi1-drive.service).
"""
import mmap, struct, os, sys

GPIOMEM = "/dev/gpiomem0"
PADS_BASE = 0x20000
MAP_SIZE = 256 * 1024
PINS = [16, 19, 20, 21]   # CS, MISO, MOSI, SCLK

fd = os.open(GPIOMEM, os.O_RDWR | os.O_SYNC)
mm = mmap.mmap(fd, MAP_SIZE, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
for pin in PINS:
    off = PADS_BASE + 4 + pin * 4
    mm.seek(off)
    val = struct.unpack("<I", mm.read(4))[0]
    new_val = (val & ~0x70) | 0x70   # DRIVE=12mA(11b=0x30) + SLEWFAST(0x40) = 0x70
    mm.seek(off)
    mm.write(struct.pack("<I", new_val))
mm.close()
os.close(fd)
print("RP1 SPI1 GPIO16/19/20/21 set to 12mA + fast slew")
sys.exit(0)
