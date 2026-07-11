# Technical Setup

## Document Status

`current`

## Function of This Document

This document answers: "what does the environment need and how is this compiled or executed".

## Objective

Collect the minimum technical requirements and steps to prepare the project.

General rule:

- The setup is only considered correct when Navio2 works as a whole for ArduRover on Pi 5: PWM, outputs, ADC, RC input, onboard GPS, and supported sensors must work through the Linux/ArduPilot stack. The LSM9DS1 accel/gyro (2nd IMU) is a known exception — deferred, and it is an ArduPilot driver limitation on Pi 5/RP1, not a hardware defect (its magnetometer works as the 2nd compass).
- Do not treat peripheral bypasses as the target state when the hardware is expected to be supported by Navio2 and ArduPilot.

## Confirmed Environment Inventory

- Main tools: g++ (aarch64, native on Pi 5), make, git
- Build environment: Raspberry Pi 5 running Raspberry Pi OS Bookworm 64-bit
- Verified dependencies: libgpiod-dev (v2+), i2c-tools, spi-tools
- Main resources validated: /dev/gpiochip0, /dev/spidev0.*, /dev/i2c-1, /sys/kernel/rcio/, /sys/class/pwm/
- Validated hardware: Navio2 HAT mounted on Raspberry Pi 5 40-pin header

## Environment Preparation

- Ensure SPI is enabled: `sudo raspi-config` → Interface Options → SPI → Yes, or add `dtparam=spi=on` to /boot/firmware/config.txt
- Ensure I2C is enabled: `sudo raspi-config` → Interface Options → I2C → Yes, or add `dtparam=i2c_arm=on` to /boot/firmware/config.txt
- Enable PWM: add `dtoverlay=pwm-2chan` to /boot/firmware/config.txt
- Install build dependencies: `sudo apt-get install -y build-essential libgpiod-dev`
- Verify GPIO chip: `gpiodetect` should show `gpiochip0` and `gpiochip4` (RP1)
- Verify SPI devices: `ls /dev/spidev*`
- Verify I2C bus: `ls /dev/i2c*`
- Verify PWM: `ls /sys/class/pwm/`

## ArduPilot Rover Build and Run

```bash
# Build (native aarch64 on Pi 5) — from ArduPilot master
cd ~/ardupilot-master
./waf configure --board navio2 --toolchain=native   # 64-bit Pi needs native; navio2 else defaults to 32-bit cross
./waf rover                                          # reconfigure required after any hwdef.dat edit
# On old 4.6.x releases also pass --define=HAL_BARO_MS5611_I2C_BUS=1 (master sets it in hwdef)

# Run manually
sudo ardurover --serial1 udp:<GCS_IP>:14550 \
  --defaults ~/ardurover_work/boat_navio2.parm

# Run via systemd
sudo systemctl start ardurover
sudo systemctl stop ardurover
sudo systemctl enable ardurover   # auto-start on boot

# Edit configuration
sudo nano /etc/default/ardurover
```

### Required ArduPilot source patches

Against current master, only **two** patches remain (both open PRs on `axonbf/ardupilot`). Sensor config needs **no** patch — master's navio2 hwdef already declares MPU9250 + LSM9DS1 mag + AK8963.

| File | Patch | Reason | PR |
|---|---|---|---|
| `AP_HAL_Linux/HAL_Linux_Class.cpp` | Runtime pwmchip detection (scan `/sys/class/pwm` for the 14-ch RCIO chip) | Pi 4 = pwmchip0, Pi 5 = pwmchip6 (RP1 shifts enumeration) | #33655 |
| `AP_HAL_Linux/PWM_Sysfs.cpp` | Retry `duty_cycle` open loop | Slow sysfs export after `pwm/export` on Pi 5 | #33656 |

**Dropped patches** (were only for old 4.6.3, validated unnecessary 2026-07-05): MS5611 CRC-skip (PROM CRC is valid on this HW), `ALLOW_NO_SENSORS` + NONE backend + panic→warn (MPU9250 works), `HAL_BARO_MS5611_I2C_BUS` (declared in hwdef).

### Boat parameter file

`~/ardurover_work/boat_navio2.parm` — migrated from Rover 4.0.0 params with:
- `SERVO1_FUNCTION 73` (Throttle), `SERVO3_FUNCTION 74` (Steering)
- `FRAME_CLASS 2` (Boat)
- `SERIAL1_PROTOCOL 2` (MAVLink telemetry)
- `SERIAL3_PROTOCOL 5` (GPS on SPI — onboard M8N)
- `BARO_PROBE_EXT 4`, `BARO_EXT_BUS 1` (MS5611 on I2C bus 1)
- `COMPASS_ENABLE 1` + `COMPASS_USE/USE2 1` — 2 compasses (AK8963 + LSM9DS1 mag via `navio2-spi0-cs2` overlay), calibrated
- `INS_ENABLE_MASK 1` — MPU9250 only (LSM9DS1 2nd IMU deferred; see below)
- `GPS_TYPE 2` (uBlox), `GPS_TYPE2 0`
- `ARMING_CHECK 1` (enabled after calibration)

## RCIO STM32F103 — SOLVED (2026-05-14)

RCIO communication on Pi 5 is working: `alive=1`, `crc=0xb9064332`, PWM/ADC/RCInput all confirmed.

**Two root causes were found and fixed:**

1. **RP1 GPIO drive strength (4 mA default → 12 mA fix)**: BCM2711 (Pi 4) defaults to 16 mA drive on all GPIO. RP1 (Pi 5) defaults to 4 mA — insufficient to drive MOSI/SCLK/CS through the Navio2 PCB traces to the STM32. Fixed via a systemd service that runs at early boot.

2. **Wrong read phase in `rcio_spi.c`**: the Pi 5 port sent 68 bytes of 0xFF on MOSI during the STM32 response read, which the STM32 interpreted as new commands. The original uses `spi_write_then_read(NULL, 0, rx=68)` — receive only, no MOSI during read.

**Deployment steps for a fresh Pi 5:**

```bash
# 1. Install the RP1 drive-strength service (run once per Pi 5)
sudo cp rp1-spi1-drive.py /usr/local/bin/
sudo cp rp1-spi1-drive.service /etc/systemd/system/
sudo systemctl enable rp1-spi1-drive.service

# 2. Add overlays to /boot/firmware/config.txt (in order)
dtoverlay=spi1-1cs,cs0_pin=16,cs0_spidev=disabled
dtoverlay=rcio-pi5

# 3. Blacklist RCIO for safe boot (manual load via rcio-startup.sh)
echo -e "blacklist rcio_spi\nblacklist rcio_core\ninstall rcio_spi /bin/true\ninstall rcio_core /bin/true" | sudo tee /etc/modprobe.d/blacklist-rcio.conf

# 4. Reboot, then run rcio-startup.sh (or start ardurover.service which calls it)
/home/pi/rcio-startup.sh
```

**Key files on Pi 5:**

- `/usr/local/bin/rp1-spi1-drive.py` — sets 12 mA on GPIO16/19/20/21
- `/etc/systemd/system/rp1-spi1-drive.service` — runs at sysinit before RCIO
- `/etc/modprobe.d/blacklist-rcio.conf` — prevents RCIO auto-load at boot
- `/home/pi/rcio-startup.sh` — loads RCIO modules + disables RT throttling
- `/home/pi/rcio_build/` — built RCIO kernel modules (`rcio_core.ko`, `rcio_spi.ko`)
- `/boot/firmware/overlays/rcio-pi5.dtbo` — RCIO device tree overlay (Pi 5, bcm2712)
- `/boot/firmware/overlays/navio2-led.dtbo` — RGB LED device tree overlay
- `/etc/systemd/system/ardurover.service` — ArduPilot auto-start service
- `/etc/default/ardurover` — ArduPilot configuration (serial ports, telemetry)
- `/etc/update-motd.d/20-navio2` — login greeting with AX ON banner
- `/home/pi/ardurover_work/boat_navio2.parm` — boat parameter file

**RCIO kernel module platform compatibility:**

The RCIO source in this repo is platform-guarded for Pi 4 / Pi 5 compatibility:
- `rcio_gpio.c`: `GPIO_CHIP_OFFSET = -1` (dynamic allocation — kernel picks free base on any platform)
- `rcio_spi.c`: CS delays via `module_param()` (defaults to 0; Pi 5 passes `cs_setup_us=50 cs_hold_us=50 cs_inactive_us=500`); `remove` return type guarded by `LINUX_VERSION_CODE >= 6.2.0`
- `rcio_pwm.c`: Uses `.apply/.get_state` API (old `.enable/.disable/.config` removed in kernel 5.13+)
- Separate overlays: `rcio-overlay.dts` (Pi 4, bcm2709) and `rcio-pi5-overlay.dts` (Pi 5, bcm2712)

## Reference Commands

```bash
# Clone and build Navio2 examples on Pi 5
git clone https://github.com/emlid/Navio2.git
cd Navio2/C++

# Build the library
make -C Navio lib

# Build all examples
make

# Run individual examples from Build/
sudo ./Build/Barometer
sudo ./Build/AccelGyroMag
sudo ./Build/GPS
sudo ./Build/ADC
sudo ./Build/Servo
sudo ./Build/RCInput
sudo ./Build/LED
```

## Reference Repositories and Links

- Emlid Navio2 C++ drivers & examples: https://github.com/emlid/Navio2
- Emlid ArduPilot fork (Navio2 board support): https://github.com/emlid/ardupilot
- Emlid blackmagic SWD tool (branch `feat/pi2`): https://github.com/emlid/blackmagic/tree/feat/pi2
- Emlid emlidtool (RCIO firmware updater): extracted to `/tmp/opencode/emlidtool_pypi/`
- RCIO kernel module source (Pi 5 port): `/home/benjaminfernandez/Projects/opencode/projects/navio2_rp5/rcio_source/src/`
- `blackmagic_pi5/` — incomplete Pi 5 SWD probe port (skeleton only, not needed for current operation)
- Navio2 pinout: https://docs.emlid.com/navio2/dev/pinout/
- Navio2 RCIO: https://docs.emlid.com/navio2/dev/rcio/
- ArduPilot Navio2 wiring: https://ardupilot.org/sub/docs/common-navio2-wiring-and-quick-start.html
- STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html
- stlink (st-flash): https://github.com/stlink-org/stlink

## Current Status

- Navio2 driver library and examples validated on Pi 5 hardware (LSM9DS1 accel/gyro deferred as 2nd IMU — driver-side, not a hardware defect; its magnetometer works)
- ArduPilot Rover (master, `V4.8.0-dev`) running on `navio2` board subtype — MS5611 barometer, MPU9250 IMU, M8N GPS (3D lock), **2 compasses (AK8963 + LSM9DS1 mag), calibrated**, 14ch PWM, ADC, RCInput all working
- ArduPilot auto-starts via systemd (`ardurover.service`) with RCIO loaded by `rcio-startup.sh`
- I2C + RCIO bus contention: **RESOLVED** — MS5611 works concurrently with RCIO
- IMU calibration done via QGC, arming checks enabled
- **Next priority**: end-to-end boat test with arming; external F9P GPS and Kogger depth sensor integration