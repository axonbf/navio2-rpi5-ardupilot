# Implementation Plan

## Document Status

`in review`

## Objective

Define the project work plan and record its technical evolution.

## Phases

### Phase 1 — Preparation ✅
- Researched upstream Navio2 codebase and dependencies
- Identified pigpio replacement strategy (libgpiod)
- Documented scope, design decisions, and risks
- Created project source tree from upstream

### Phase 2 — Driver Port ✅
- Rewrote `Navio/Common/gpio.cpp` — replaced `/dev/mem` mmap with libgpiod v2
- Updated Makefiles — removed pigpio, added `-lgpiod`, targeted aarch64
- Updated `get_dependencies.sh` — libgpiod-dev
- Ported RCIO kernel module to Linux 6.6 API

### Phase 3 — Validation ✅
- Built on Pi 5 hardware (native aarch64)
- Verified /dev/gpiochip, SPI, I2C, PWM device presence
- Tested all example binaries:
  - Barometer (MS5611, I2C): **940 mbar, 32.8°C** ✅
  - AccelGyroMag (MPU9250): **accel/gyro/mag live** ✅
  - AccelGyroMag (LSM9DS1): accel/gyro **deferred as 2nd IMU** (driver-side on Pi 5/RP1, not a hardware defect — raw SPI reads valid data); magnetometer works as 2nd compass
  - GPS (U-blox M8N): **NMEA data received** ✅
  - LED: **GPIO via libgpiod** ✅
  - ADC: **/sys/kernel/rcio/adc/ch0-5 responding** ✅
  - RCInput: **/sys/kernel/rcio/rcin/ch0-15 responding** ✅
  - Servo: **/sys/class/pwm/pwmchip, 50 Hz confirmed** ✅

### Phase 4 — Hailo AI + Camera + ROS2 (in progress)
- Hailo-8L M.2 detected via PCIe, /dev/hailo0 available ✅
- HailoRT 4.18, hailo-tappas-core 3.29.1, rpicam-apps installed ✅
- ROS2 build tools installed ✅
- HP 320 FHD Webcam working (/dev/video0) ✅
- Camera preview working ✅
- [ ] ROS2 camera topic publishing
- [ ] Hailo inference pipeline over ROS2
- [ ] Navio2 sensor data via ROS2

### Phase 5 — ArduPilot Rover on Pi 5 ✅

#### Completed
- ArduPilot Rover built for `navio2` board subtype ✅ (originally 4.6.3; **current binary is master `V4.8.0-dev` in `~/ardupilot-master`** — build from there)
- MS5611 barometer: working on I2C bus 1 with RCIO loaded ✅
- U-blox M8N GPS: detected via SPI, 11 sats / 3D lock ✅
- MPU9250 IMU: working, calibrated via QGC ✅
- **RCIO STM32F103: SOLVED 2026-05-14** — `alive=1`, `crc=0xb9064332` ✅
  - Root cause 1: RP1 GPIO 4 mA default → fixed with 12 mA service
  - Root cause 2: ported `rcio_spi.c` sent 0xFF on MOSI during read → fixed to RX-only
- PWM (14ch), ADC (6ch), RCInput (16ch) all verified via ArduPilot ✅
- **I2C + RCIO bus contention: RESOLVED** — MS5611 works concurrently with RCIO ✅
- **Navio2 RGB LED: RESOLVED** — `GPIO_CHIP_OFFSET = -1` (dynamic allocation), `navio2-led.dtbo` device tree overlay creates `/sys/class/leds/rgb_led{0,1,2}` ✅
- Boat parameters migrated from Rover 4.0.0 to 4.6.3 (`boat_navio2.parm`) ✅
- **Systemd service for ArduRover auto-start** ✅
  - `/etc/systemd/system/ardurover.service` — starts after `rp1-spi1-drive.service`, loads RCIO via `rcio-startup.sh`
  - `/etc/default/ardurover` — same format as Pi 4, uses `--serialN` syntax (ArduPilot 4.6.3)
  - `/home/pi/rcio-startup.sh` — loads RCIO modules + disables RT throttling
- Login greeting (`/etc/update-motd.d/20-navio2`) with AX ON banner ✅

#### Remaining
- [ ] **End-to-end boat test**: GPS + sensors + RC input + PWM servos with arming
- [ ] **External F9P GPS** on `/dev/ttyACM0` (`SERIAL3`) when connected
- [ ] **Kogger depth sounder** on `/dev/ttyUSB0` (`SERIAL4`) when connected
- [ ] **Clean up Pi 5 home directory** (temp files, old logs)
- [ ] **Publish solution**: fork emlid/rcio + ArduPilot PRs + forum posts
  - RCIO PR #11 (bugfixes): [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) — safe for all platforms
  - RCIO PR #12 (Pi 5 support): [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) — dynamic GPIO base, module_param CS delays
  - ArduPilot PR #33655 (RCIO pwmchip runtime detect, change C): [ArduPilot/ardupilot#33655](https://github.com/ArduPilot/ardupilot/pull/33655) — clean, against master, HW-tested
  - ArduPilot PR #33656 (PWM_Sysfs duty_cycle retry, change G): [ArduPilot/ardupilot#33656](https://github.com/ArduPilot/ardupilot/pull/33656) — clean, against master, HW-tested
  - Public repo: [axonbf/navio2-rpi5-ardupilot](https://github.com/axonbf/navio2-rpi5-ardupilot) — full setup guide, scripts, overlays, docs
  - Old PRs closed: emlid/rcio-dkms#10, ArduPilot/ardupilot#33645, **#33647 + #33648** (unguarded 4.6.3 drafts, superseded by #33655/#33656)
  - Forum posts: Emlid community + ArduPilot Discourse

#### ArduPilot run commands

```bash
# Manual start (for testing)
sudo ardurover --serial1 udp:<GCS_IP>:14550 \
  --defaults ~/ardurover_work/boat_navio2.parm

# Systemd start/stop/enable
sudo systemctl start ardurover
sudo systemctl stop ardurover
sudo systemctl enable ardurover   # auto-start on boot

# Edit configuration
sudo nano /etc/default/ardurover
```

#### Required ArduPilot source patches (against master)

Only two remain; the old 4.6.3 patch set was dropped as unnecessary (validated 2026-07-05). Sensor config needs no patch — master's navio2 hwdef declares MPU9250 + LSM9DS1 mag + AK8963.

| File | Patch | Reason | PR |
|---|---|---|---|
| `AP_HAL_Linux/HAL_Linux_Class.cpp` | Runtime pwmchip detection | Pi 4 = pwmchip0, Pi 5 = pwmchip6 | #33655 |
| `AP_HAL_Linux/PWM_Sysfs.cpp` | Retry `duty_cycle` open | Slow sysfs export on Pi 5 | #33656 |

**Dropped:** MS5611 CRC-skip (PROM CRC valid on HW), `ALLOW_NO_SENSORS` + NONE backend + panic→warn (MPU9250 works), `HAL_BARO_MS5611_I2C_BUS` (in hwdef).

## Key files on Pi 5

| File | Purpose |
|---|---|
| `/etc/systemd/system/ardurover.service` | ArduPilot auto-start service |
| `/etc/default/ardurover` | ArduPilot configuration (serial ports, telemetry) |
| `/home/pi/rcio-startup.sh` | RCIO module load + RT throttling disable |
| `/home/pi/ardurover_work/boat_navio2.parm` | Boat parameter file (used with master; `INS_ENABLE_MASK 1`, 2 compasses enabled) |
| `/usr/local/bin/rp1-spi1-drive.py` | 12 mA drive-strength fix for RP1 SPI1 pins |
| `/etc/systemd/system/rp1-spi1-drive.service` | Runs at sysinit before RCIO loads |
| `/etc/modprobe.d/blacklist-rcio.conf` | Prevents RCIO auto-load at boot |
| `/etc/update-motd.d/20-navio2` | Login greeting with AX ON banner |
| `/home/pi/rcio_build/` | Built RCIO kernel modules |
| `/boot/firmware/overlays/rcio-pi5.dtbo` | RCIO SPI1 device-tree overlay (Pi 5, bcm2712) |
| `/boot/firmware/overlays/navio2-led.dtbo` | RGB LED device-tree overlay (GPIO4/6/27) |
| `/boot/firmware/config.txt` | Contains `dtoverlay=spi1-1cs,...`, `dtoverlay=rcio-pi5`, `dtoverlay=navio2-led` |