# Navio2 + RP5 Project

Porting Navio2 autopilot HAT drivers to Raspberry Pi 5 (64-bit Bookworm, RP1 peripheral controller), with Hailo-8L AI accelerator, camera, and ROS2 integration. Target application: autonomous boat using ArduPilot Rover firmware.

## Project Structure

- `src/` — Navio2 C++ driver library (libnavio.a) and examples (ported to libgpiod)
- `upstream/` — Reference clone of emlid/Navio2 upstream
- `rcio_source/` — RCIO kernel module source (Pi 5 port, platform-guarded)
- `docs/` — Project documentation (see docs/README.md for rules)

## Build Commands

```bash
# Build Navio2 library and examples (on Pi, native aarch64)
cd src && make -C Navio lib && make
```

```bash
# Build ArduPilot Rover (on Pi, native aarch64)
cd ~/ardupilot
./waf configure --board navio2 --define=HAL_BARO_MS5611_I2C_BUS=1
./waf build -j4
```

## Run/Validation Commands

```bash
# ArduPilot Rover (manual start)
sudo ardurover --serial1 udp:<GCS_IP>:14550 --defaults ~/ardurover_work/boat_navio2.parm

# ArduPilot Rover (systemd)
sudo systemctl start ardurover    # start now
sudo systemctl stop ardurover     # stop now
sudo systemctl enable ardurover   # start on boot

# Edit config
sudo nano /etc/default/ardurover

# Navio2 examples (require sudo for GPIO/SPI/I2C)
sudo ./Build/Barometer
sudo ./Build/AccelGyroMag
sudo ./Build/GPS
sudo ./Build/ADC
sudo ./Build/Servo
sudo ./Build/RCInput
sudo ./Build/LED
```

## Architecture

- GPIO: `libgpiod` v2 via `/dev/gpiochip0` (replaces pigpio which is incompatible with Pi 5 RP1)
- SPI: Standard `/dev/spidev*` — unchanged from upstream
- I2C: Standard `/dev/i2c-1` — MS5611 works concurrently with RCIO (contention resolved)
- RCIO: **Blacklisted at boot** via `/etc/modprobe.d/blacklist-rcio.conf`; loaded by `rcio-startup.sh` before ArduPilot starts.
- RCIO STM32F103: **SOLVED on Pi 5 (2026-05-14)** — `alive=1`, `board_name=navio2`, `crc=0xb9064332`. Two root causes:
  1. RP1 GPIO defaults to 4 mA drive (BCM2711 uses 16 mA) — fixed via `rp1-spi1-drive.service` setting 12 mA on GPIO16/19/20/21.
  2. Ported `rcio_spi.c` sent 0xFF bytes on MOSI during read phase — original uses RX-only read. Fixed by restoring original `wait_complete()`.
- RCIO SPI pins on Navio2 HAT: GPIO16=CS (pin 36), GPIO19=MISO (pin 35), GPIO20=MOSI (pin 38), GPIO21=SCLK (pin 40)
- RCIO GPIO base: dynamic allocation (`GPIO_CHIP_OFFSET = -1`) — kernel picks free base on any platform
- RCIO PWM API migrated from `.enable/.disable/.config` to `.apply/.get_state` (kernel 5.13+ removed old API)
- RCIO SPI CS delays via `module_param()` (defaults to 0; Pi 5 passes `cs_setup_us=50 cs_hold_us=50 cs_inactive_us=500`)
- RCIO `spi_driver.remove` return type guarded by `LINUX_VERSION_CODE >= 6.2.0`
- Separate device tree overlays: `rcio-overlay.dts` (Pi 4, bcm2709) and `rcio-pi5-overlay.dts` (Pi 5, bcm2712)
- Navio2 RGB LED: device tree overlay `navio2-led.dtbo` creates `/sys/class/leds/rgb_led{0,1,2}` from GPIO4/6/27
- Navio2 compass: `navio2-spi0-cs2.dtbo` adds a 3rd SPI0 chip-select on GPIO22 → `/dev/spidev0.2` → LSM9DS1 magnetometer + AK8963 both detected as compasses. Pi 5 SPI0 exposes only 2 CS by default; without it, enabling the compass panics on the missing `/dev/spidev0.2`.
- Pi 5 RCIO SPI1 uses RP1 `/axi/pcie@120000/rp1/spi@54000` with `dw_spi_mmio` / `spi_dw`; Pi 4 known-good uses BCM SoC SPI at `/soc/...spi@7e215080`
- Pi 3 second-HAT known-good uses BCM aux SPI at `/soc/spi@7e215080`, `spi1.0`, `alive=1`, `board_name=navio2`, `crc=0xb9064332`, git hash `7851d1a`, `pwm_ok=1`
- **No Pi-controlled NRST or BOOT0 pins on Navio2 HAT** — NRST hardwired HIGH (pull-up to 3.3V), BOOT0 tied to GND

## Key Conventions

- C++ standard: C++11 (`-std=c++11`)
- Target: aarch64 only (Pi 5 native compilation)
- Sensor status: MPU9250 IMU + MS5611 baro + GPS + 2 compasses (LSM9DS1 magnetometer + AK8963, via `navio2-spi0-cs2` overlay) working. LSM9DS1 accel/gyro reads WHO_AM_I = 0xFF and is skipped (real defect vs. missing chip-select on GPIO25 under investigation)
- Remote target: `ssh pi@<PI5_IP>` (replace with your Pi 5 IP)

## Current Phase

- Phase 1-3: Navio2 driver port COMPLETE
- Phase 4: Hailo AI + Camera + ROS2 integration IN PROGRESS
- Phase 5: ArduPilot Rover on Pi 5 — **COMPLETE** (GPS 3D lock, IMU calibrated, PWM/ADC/RCIO working, RGB LED, systemd service)
- Next step: end-to-end boat test with arming; external F9P GPS and Kogger depth sensor integration; clean up Pi 5 home directory; PRs open for review (emlid/rcio-dkms#11, #12, ArduPilot/ardupilot#33647, #33648).

## Communication Rules

1. **No post-hoc rationalization**: When asked "why did you do X?", state what you actually thought at the time — not a reconstructed plausible answer.
2. **No telling the user what they want to hear**: Answer honestly, even if the answer makes you look wrong or incompetent. Do not be condescending, patronizing, obliging, or accommodating.
3. **If you give a different method than the one you used, say why transparently**: "I used `ps` in my script because X, but suggested `top` to you because Y" — not hiding inconsistency.

## Validation Rule

**Always validate before committing.** Never blindly commit changes. Before every commit:
1. Build the code on the target hardware (Pi 5) and verify it compiles.
2. If the change affects runtime behavior, test it (load modules, run ArduPilot, verify sensors).
3. Only commit after validation passes.

## Change Application Rule

**Always ask before applying changes to the Pi 5.** Do not push files, modify source, or rebuild without explicit user approval.

## Plan-and-Approve Rule (STRICT — do not violate)

**Never take a state-changing action without an explicit, per-step approval.** State-changing = edit/revert code, build, run on the Pi, git commit, push, edit files.

1. Before such an action, present a CONCRETE plan: exact file(s), exact command(s), exact expected result. Then STOP and wait for a yes to THAT step.
2. Approval for one step is NOT approval for the next. "Go ahead", "finish the table", "yes" to step N never authorizes step N+1.
3. Never assume consent. If unsure whether something is approved, ask — do not act.
4. One topic at a time. Do not mix topics. Keep answers concrete, no filler.

## Documentation Rules

When making changes, update docs/ files following the rules in docs/README.md. Maintain cross-consistency between TECHNICAL_ARCHITECTURE.md, IMPLEMENTATION_PLAN.md, and TECHNICAL_SETUP.md.
