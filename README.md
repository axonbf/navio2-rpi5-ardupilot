# Navio2 + Raspberry Pi 5 + ArduPilot Rover

Full port of the Emlid Navio2 autopilot HAT to Raspberry Pi 5 (RP1 peripheral controller, aarch64, kernel 6.6).

## What this enables

- ArduPilot Rover 4.6.3 on Pi 5 with `navio2` board subtype
- MS5611 barometer (I2C)
- U-blox M8N GPS (SPI, 3D lock)
- MPU9250 IMU (SPI, calibrated via QGC)
- RCIO coprocessor: PWM ×14, ADC ×6, RC input ×16
- RGB LED status indicator

## Quick start

See [docs/QUICK_START.md](docs/QUICK_START.md) for full setup instructions.

## Repository contents

| Path | Purpose |
|---|---|
| `src/rp1-spi1-drive.py` | RP1 12 mA drive-strength fix (critical for RCIO) |
| `src/rcio-startup.sh` | RCIO module loader + RT throttling disable |
| `src/ardurover.service` | systemd service for ArduPilot auto-start |
| `src/ardurover.defaults` | ArduPilot configuration template |
| `src/boat_navio2.parm` | Boat parameter file for Rover 4.6.3 |
| `src/20-navio2` | Login greeting with AX ON banner |
| `rcio_source/` | RCIO kernel module source (Pi 5 patches) |
| `rcio_source/rcio-pi5-overlay.dts` | Device tree overlay for Pi 5 RCIO |
| `rcio_source/navio2-led.dts` | Device tree overlay for RGB LED |
| `docs/` | Full documentation with architecture diagrams |

## Related PRs

- [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) — Bugfixes (PWM API, error path, probe abort)
- [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) — Pi 5 platform support (dynamic GPIO base, module_param CS delays)
- [ArduPilot/ardupilot#33647](https://github.com/ArduPilot/ardupilot/pull/33647) — Linux bugfixes (PWM_Sysfs retry, INS NONE backend)
- [ArduPilot/ardupilot#33648](https://github.com/ArduPilot/ardupilot/pull/33648) — Navio2 Pi 5 patches (CRC skip, allow no sensors, pwmchip, native toolchain)

## Tested hardware

- Raspberry Pi 5 (8GB)
- Navio2 autopilot HAT
- Raspberry Pi OS Bookworm 64-bit
- Kernel 6.6.51+rpt-rpi-2712
- ArduPilot Rover 4.6.3

## Known limitation

LSM9DS1 IMU has a hardware defect on this HAT (WHO_AM_I = 0xFF). ArduPilot auto-skips it; MPU9250 is used as primary IMU.

## License

Same as upstream Emlid Navio2 and ArduPilot (GPL v2/v3 respectively).
