#!/usr/bin/env python3
"""RCIO SPI protocol test for Navio2 STM32F103 on Pi 5."""

import spidev
import struct
import time
import sys

# RCIO protocol constants
PKT_CODE_READ    = 0x00
PKT_CODE_WRITE   = 0x40
PKT_CODE_SUCCESS = 0x00
PKT_CODE_CORRUPT = 0x40
PKT_CODE_ERROR   = 0x80
PKT_CODE_MASK    = 0xC0
PKT_COUNT_MASK   = 0x3F
PKT_MAX_REGS     = 32

# CRC8 table
CRC8_TAB = [
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3,
]

def crc_packet(count_code, page, offset, regs):
    pkt = struct.pack('<BBBB', count_code, 0, page, offset)
    for i in range(PKT_MAX_REGS):
        if i < len(regs):
            pkt += struct.pack('<H', regs[i])
        else:
            pkt += struct.pack('<H', 0)
    c = 0
    end = 4 + 2 * (count_code & PKT_COUNT_MASK)
    for b in pkt[:end]:
        c = CRC8_TAB[c ^ b]
    return c

def make_packet(code, count, page, offset, regs=None):
    if regs is None:
        regs = []
    count_code = (count & PKT_COUNT_MASK) | (code & PKT_CODE_MASK)
    crc = crc_packet(count_code, page, offset, regs)
    pkt = struct.pack('<BBBB', count_code, crc, page, offset)
    for i in range(PKT_MAX_REGS):
        if i < len(regs):
            pkt += struct.pack('<H', regs[i])
        else:
            pkt += struct.pack('<H', 0x55AA)
    return pkt

def parse_packet(data):
    if len(data) < 4:
        return None
    count_code, crc, page, offset = struct.unpack('<BBBB', data[:4])
    regs = []
    count = count_code & PKT_COUNT_MASK
    code = count_code & PKT_CODE_MASK
    for i in range(PKT_MAX_REGS):
        if 4 + i*2 + 1 < len(data):
            regs.append(struct.unpack('<H', data[4+i*2:6+i*2])[0])
        else:
            regs.append(0)
    return {'code': code, 'count': count, 'crc': crc, 'page': page, 'offset': offset, 'regs': regs}

def rcio_read(spi, page, offset, count):
    pkt = make_packet(PKT_CODE_READ, count, page, offset)
    # Phase 1: send read request
    spi.xfer2(list(pkt))
    # Phase 2: wait for response, send dummy
    time.sleep(0.001)
    rx = spi.xfer2([0xFF] * len(pkt))
    return parse_packet(bytes(rx))

def rcio_write(spi, page, offset, regs):
    count = len(regs)
    pkt = make_packet(PKT_CODE_WRITE, count, page, offset, regs)
    spi.xfer2(list(pkt))
    time.sleep(0.001)
    rx = spi.xfer2([0xFF] * len(pkt))
    resp = parse_packet(bytes(rx))
    # Phase 3: read result
    time.sleep(0.001)
    rx2 = spi.xfer2([0xFF] * len(pkt))
    return parse_packet(bytes(rx2))

def main():
    spi = spidev.SpiDev(1, 0)

    print("=== RCIO SPI Protocol Test for Navio2 on Pi 5 ===")
    print(f"SPI1 device: /dev/spidev1.0, CS on GPIO 16")

    # Test with different speeds and modes
    for speed in [4000000, 1000000, 500000, 100000]:
        for mode in [0, 3]:
            spi.max_speed_hz = speed
            spi.mode = mode
            print(f"\n--- Speed: {speed} Hz, Mode: {mode} ---")

            # Read heartbeat (page 20, offset 0, count 1)
            print("Reading heartbeat (page=20, offset=0, count=1)...")
            resp = rcio_read(spi, page=20, offset=0, count=1)
            if resp:
                print(f"  code=0x{resp['code']:02x} count={resp['count']} page={resp['page']} offset={resp['offset']}")
                print(f"  regs[0:4] = {['0x%04x' % r for r in resp['regs'][:4]]}")
                if resp['code'] == PKT_CODE_SUCCESS and resp['count'] > 0:
                    print(f"  HEARTBEAT alive = {resp['regs'][0]}")
            else:
                print("  No response")

            # Read status/alive (page=1, offset=2, count=1)
            print("Reading status flags (page=1, offset=2, count=1)...")
            resp = rcio_read(spi, page=1, offset=2, count=1)
            if resp:
                print(f"  code=0x{resp['code']:02x} count={resp['count']} regs[0]=0x{resp['regs'][0]:04x}")

            # Read config/board type (page=0, offset=10, count=1)
            print("Reading board type (page=0, offset=10, count=1)...")
            resp = rcio_read(spi, page=0, offset=10, count=1)
            if resp:
                print(f"  code=0x{resp['code']:02x} count={resp['count']} regs[0]=0x{resp['regs'][0]:04x}")

            # Just send raw bytes and see what comes back
            print("Raw SPI transfer (all zeros)...")
            spi.max_speed_hz = speed
            spi.mode = mode
            raw = spi.xfer2([0x00] * 68)
            print(f"  First 16 bytes: {' '.join(f'{b:02x}' for b in raw[:16])}")

            raw = spi.xfer2([0x00] * 68)
            print(f"  Second 16 bytes: {' '.join(f'{b:02x}' for b in raw[:16])}")

    spi.close()
    print("\n=== Test complete ===")

if __name__ == '__main__':
    main()