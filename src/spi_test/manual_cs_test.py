#!/usr/bin/env python3

import subprocess
import time

import spidev


CS_GPIO = 16


def run(cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def set_cs(level):
    mode = "dh" if level else "dl"
    run(["sudo", "pinctrl", str(CS_GPIO), "op", mode])


def setup_cs():
    set_cs(1)


def xfer_manual_cs(spi, tx):
    set_cs(0)
    time.sleep(0.001)
    rx = spi.xfer2(tx)
    time.sleep(0.001)
    set_cs(1)
    time.sleep(0.001)
    return rx


def fmt(buf):
    return " ".join(f"{b:02x}" for b in buf[:16])


def main():
    spi = spidev.SpiDev(1, 0)
    setup_cs()

    print("=== Manual CS SPI1 Test ===")
    print("device=/dev/spidev1.0 hardware CS plus manual GPIO16")

    for speed in [4_000_000, 1_000_000, 500_000, 100_000]:
        for mode in [0, 3]:
            spi.max_speed_hz = speed
            spi.mode = mode
            print(f"\n--- speed={speed} mode={mode} ---")

            r1 = xfer_manual_cs(spi, [0x00] * 32)
            r2 = xfer_manual_cs(spi, [0x00] * 32)
            print("zeros1:", fmt(r1))
            print("zeros2:", fmt(r2))

            pkt = [0x01, 0x81, 0x14, 0x00] + [0x00] * 28
            r3 = xfer_manual_cs(spi, pkt)
            r4 = xfer_manual_cs(spi, [0xFF] * 32)
            print("readpkt:", fmt(r3))
            print("clock :", fmt(r4))

    spi.close()


if __name__ == "__main__":
    main()
