#!/usr/bin/env python3

import os
import struct
import subprocess
import time

import spidev


SPI_DEV = "/sys/bus/spi/devices/spi1.0"
SPIDEV_DRIVER = "/sys/bus/spi/drivers/spidev"
SPI_NODE = "/proc/device-tree/axi/pcie@120000/rp1/spi@54000"
RCIO_NODE = SPI_NODE + "/rcio@0"


def run(cmd, check=True):
    return subprocess.run(cmd, check=check, text=True, capture_output=True)


def write(path, value):
    with open(path, "w") as f:
        f.write(value)


def unbind_spidev():
    if os.path.exists(SPI_DEV + "/driver"):
        try:
            write(SPIDEV_DRIVER + "/unbind", "spi1.0")
        except Exception:
            pass


def bind_spidev():
    write(SPI_DEV + "/driver_override", "spidev")
    write(SPIDEV_DRIVER + "/bind", "spi1.0")


def read_u32_prop(path):
    if not os.path.exists(path):
        return None
    data = open(path, "rb").read()
    if len(data) < 4:
        return None
    return struct.unpack(">I", data[:4])[0]


def apply_overlay(delay_ns, cs_delay_ns):
    dts = f"""/dts-v1/;
/plugin/;

/ {{
    compatible = "brcm,bcm2712";

    fragment@0 {{
        target = <&spi1>;
        __overlay__ {{
            rx-sample-delay-ns = <{delay_ns}>;

            rcio@0 {{
                rx-sample-delay-ns = <{delay_ns}>;
                spi-cs-setup-delay-ns = <{cs_delay_ns}>;
                spi-cs-hold-delay-ns = <{cs_delay_ns}>;
                spi-cs-inactive-delay-ns = <{cs_delay_ns}>;
            }};
        }};
    }};
}};
"""
    base = f"/tmp/rxdelay_{delay_ns}_{cs_delay_ns}"
    dts_path = base + ".dts"
    dtbo_path = base + ".dtbo"
    with open(dts_path, "w") as f:
        f.write(dts)
    run(["dtc", "-@", "-I", "dts", "-O", "dtb", "-o", dtbo_path, dts_path])
    run(["sudo", "dtoverlay", dtbo_path])


def fmt(buf):
    return " ".join(f"{b:02x}" for b in buf[:8])


def classify(buf):
    if all(b == 0x00 for b in buf):
        return "all00"
    if all(b == 0xFF for b in buf):
        return "allff"
    return fmt(buf)


def run_spi_tests():
    spi = spidev.SpiDev(1, 0)
    results = []
    payloads = [
        ("zero", [0x00] * 32),
        ("ff", [0xFF] * 32),
        ("read", [0x01, 0x81, 0x14, 0x00] + [0x00] * 28),
    ]
    for mode in [0, 3]:
        spi.mode = mode
        for speed in [4_000_000, 1_000_000, 100_000, 10_000]:
            spi.max_speed_hz = speed
            line = [f"mode={mode}", f"speed={speed}"]
            for name, payload in payloads:
                rx = spi.xfer2(payload)
                time.sleep(0.001)
                line.append(f"{name}={classify(rx)}")
            results.append(" ".join(line))
    spi.close()
    return results


def main():
    print("=== RP1 SPI rx-sample-delay sweep ===")
    print("Note: runtime overlays accumulate until reboot; property values are read back after each apply.")
    delays = [None, 0, 1, 2, 5, 10, 20, 50, 75, 100, 150, 200, 300, 500, 1000]
    cs_delay_ns = 1000

    for delay in delays:
        print("\n=== delay=%s ===" % ("baseline" if delay is None else str(delay)))
        unbind_spidev()
        if delay is not None:
            apply_overlay(delay, cs_delay_ns)
        bind_spidev()
        controller_val = read_u32_prop(SPI_NODE + "/rx-sample-delay-ns")
        rcio_val = read_u32_prop(RCIO_NODE + "/rx-sample-delay-ns")
        print(f"controller_rx_sample_delay_ns={controller_val}")
        print(f"rcio_rx_sample_delay_ns={rcio_val}")
        for line in run_spi_tests():
            print(line)

    unbind_spidev()
    write(SPI_DEV + "/driver_override", "")
    print("\nspidev unbound and driver_override cleared")


if __name__ == "__main__":
    main()
