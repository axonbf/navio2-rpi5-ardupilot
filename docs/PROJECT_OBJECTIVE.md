# Project Objective

## Document Status

`current`

## Purpose of This Document

This document defines two key things:
- What we want to achieve: the expected functional result of the project.
- Where the scope ends: what is inside and outside this phase.

## Objective

Make the Navio2 autopilot HAT work properly on Raspberry Pi 5 with current ArduPilot Rover, preserving full Navio2 peripheral support under Linux on Pi 5.

This includes the full Navio2 HAT surface expected by ArduPilot and by the Navio2 developer examples:
- PWM and output channels
- ADC
- RC input
- onboard GPS
- supported IMU, barometer, and other onboard sensors
- the Linux-side device access and board configuration ArduPilot expects

The only accepted functional exception is the LSM9DS1 accel/gyro as a 2nd IMU — deferred, and it is an ArduPilot driver limitation on Pi 5/RP1, not a hardware defect (the chip reads valid data over raw SPI; its magnetometer works as the 2nd compass).

## Scope

- Replace pigpio dependency with native GPIO access compatible with Pi 5's RP1 chip (gpiod/libgpiod or direct /dev/gpiochip)
- Adapt device tree overlay for Navio2 HAT (SPI, I2C, PWM configuration) to work with Pi 5 kernel
- Port the C++ Navio2 driver library (Common + Navio2 subdirectories) to compile natively on Pi 5 aarch64
- Port C++ examples (AccelGyroMag, ADC, AHRS, Barometer, GPS, LED, RCInput, Servo) to build and run
- Verify sensor communication: MPU9250 (SPI), LSM9DS1 (SPI), MS5611 (I2C), U-blox GPS (SPI)
- Verify RCIO co-processor communication for PWM output and RC input
- Configure Linux, Raspberry Pi 5, and ArduPilot so current ArduRover works with the Navio2 HAT as a real supported board, not only as a partial or bypassed bring-up
- Allow ArduPilot code changes when they are required to restore or modernize Navio2 support on Raspberry Pi 5 and newer ArduPilot versions

## What We Want to Validate

- SPI devices (MPU9250, LSM9DS1, U-blox GPS) enumerate and communicate correctly via /dev/spidev*
- I2C barometer (MS5611) reads pressure/temperature via /dev/i2c*
- RCIO protocol over SPI works on the RP1-controlled SPI bus
- PWM output channels generate correct servo signals
- RC input reads correct channel values
- ADC reads valid analog values

## Constraints And Priorities

- Priority one is Linux and Raspberry Pi 5 configuration so Navio2 support works the way ArduRover expects.
- The project must not solve missing support by bypassing Navio2 peripherals that are expected to work on a healthy HAT.
- **No external-hardware fallback is allowed.** No PCA9685 PWM controller, no external ADC (MCP3008 / ADS1115 outside the Navio2 board), no Pololu Maestro, no alternative servo / RC hardware. The Navio2 HAT — including the RCIO STM32F103 — must work natively on Raspberry Pi 5, or the project terminates with a documented hardware-level incompatibility proof.
- ArduPilot code changes are allowed when they are specifically needed for Navio2 support on Raspberry Pi 5 and current ArduPilot versions.
- Those ArduPilot-side changes should be kept traceable and, when practical, shaped so they could be proposed back to the wider ArduPilot/Navio2 community.

## Termination Condition

The project terminates only in one of two ways:

1. **Success**: Navio2 RCIO produces `alive=1` and matching CRC (`0xb9064332`) on Pi 5, ArduRover binds the Navio2 board subtype, and all expected peripherals function (the LSM9DS1 accel/gyro is deferred as a 2nd IMU — an ArduPilot driver limitation on Pi 5/RP1, not a hardware defect).
2. **Documented hardware incompatibility**: after exhausting software-level fixes (kernel SPI driver patches, pinctrl adjustments, clock-domain investigation) and a hardware-level signal capture comparison (Pi 5 vs Pi 4 known-good), produce a written technical conclusion identifying the specific RP1 / DW SPI property that prevents communication with the Navio2 RCIO STM32F103, and stop.

No intermediate "partial workaround" state is an acceptable project outcome.

## Out of Scope For Now

- Performance benchmarking or real-time guarantees

## Expected Result

- A build system that produces `libnavio.a` on Pi 5 aarch64 — DONE
- All 8 C++ example binaries compile and execute without crashes — DONE (LSM9DS1 accel/gyro deferred as 2nd IMU — driver-side, not a hardware defect; its magnetometer works)
- Functional validation confirmed via serial output — DONE
- Hailo-8L M.2 AI accelerator + camera + ROS2 integration in progress

## Current Status

- Navio2 low-level port and examples are validated on Pi 5 hardware.
- ArduPilot Rover 4.6.3 running under `navio2` board subtype on Pi 5 with working MS5611 barometer and onboard GPS SPI traffic.
- **RCIO STM32F103 communication: SOLVED (2026-05-14).** `alive=1`, `board_name=navio2`, `crc=0xb9064332` confirmed on Pi 5 matching Pi 3/Pi 4 baseline. Two root causes identified and fixed:
  1. **RP1 GPIO drive strength**: RP1 defaults to 4 mA; BCM2711 (Pi 4) uses 16 mA. Fixed via `rp1-spi1-drive.service` which sets GPIO16/19/20/21 to 12 mA at boot before RCIO modules load.
  2. **Wrong read phase in ported `rcio_spi.c`**: the port sent 68 bytes of 0xFF on MOSI during the response read, confusing the STM32. Original uses `spi_write_then_read(NULL, 0, rx=68)` — receive-only. Fixed by restoring the original `wait_complete()` logic with 120 µs delays.
- **Next**: run PWM, ADC, and RCInput subsystem tests; integrate RCIO blacklist removal into ArduPilot startup; close Phase 5.
