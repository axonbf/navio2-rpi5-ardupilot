#!/usr/bin/env python3

import spidev
import time


def fmt(buf, n=16):
    return " ".join(f"{b:02x}" for b in buf[:n])


def xfer_with_pause(spi, tx, pause_s):
    rx = spi.xfer2(tx)
    if pause_s:
        time.sleep(pause_s)
    return rx


def main():
    spi = spidev.SpiDev(1, 0)

    speeds = [4_000_000, 1_000_000, 500_000, 100_000, 50_000, 10_000, 5_000, 1_000]
    pauses = [0.0, 0.0005, 0.001, 0.005, 0.01]
    modes = [0, 3]
    payloads = [
        ("zeros32", [0x00] * 32),
        ("ff32", [0xFF] * 32),
        ("alt32", [0xAA, 0x55] * 16),
        ("rcio_read_32", [0x01, 0x81, 0x14, 0x00] + [0x00] * 28),
        ("rcio_write_32", [0x41, 0x00, 0x16, 0x00] + [0x00] * 28),
    ]

    print("=== RCIO SPI Timing Matrix on /dev/spidev1.0 ===")
    print("Testing low speeds, pauses, and transaction shapes")

    for mode in modes:
        spi.mode = mode
        print(f"\n===== MODE {mode} =====")
        for speed in speeds:
            spi.max_speed_hz = speed
            print(f"\n--- speed={speed} ---")
            for pause_s in pauses:
                print(f"pause={pause_s:.4f}s")
                for name, payload in payloads:
                    r1 = xfer_with_pause(spi, payload, pause_s)
                    r2 = xfer_with_pause(spi, payload, pause_s)
                    print(f"  {name:13s} r1={fmt(r1, 8)} r2={fmt(r2, 8)}")

            # byte-at-a-time clocking at this speed
            r = []
            for b in [0x00] * 16:
                r.extend(spi.xfer2([b]))
                time.sleep(0.001)
            print(f"bytewise_zero   r={fmt(r, 16)}")

            r = []
            for b in [0xFF] * 16:
                r.extend(spi.xfer2([b]))
                time.sleep(0.001)
            print(f"bytewise_ff     r={fmt(r, 16)}")

    spi.bits_per_word = 16
    for mode in modes:
        spi.mode = mode
        for speed in [100_000, 10_000, 1_000]:
            spi.max_speed_hz = speed
            print(f"\n===== 16-bit mode={mode} speed={speed} =====")
            try:
                r = spi.xfer2([0x00, 0x00] * 16)
                print(f"16bit zeros r={fmt(r, 16)}")
            except Exception as e:
                print(f"16bit zeros error={e}")
            try:
                r = spi.xfer2([0xFF, 0xFF] * 16)
                print(f"16bit ffs   r={fmt(r, 16)}")
            except Exception as e:
                print(f"16bit ffs error={e}")

    spi.close()


if __name__ == "__main__":
    main()
