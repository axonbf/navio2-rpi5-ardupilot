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
- RCIO kernel 6.12 fixes: `pwmchip_alloc`/`pwmchip_put` (embedded `struct device` + flexible `pwms[]`), `gpiochip_add_data`, `pwm_ops.owner` removed — all guarded by `LINUX_VERSION_CODE >= 6.12.0`. Fresh Bookworm ships **kernel 6.12.93**; validated on a clean SD card (opencode session 14). Also fixed: firmware CRC sign-extension (`status/crc` now prints `0xb9064332`, was `0xffffffffb9064332` — cast `regs[1]` to `uint32_t`) and removed dead `to_rcio_pwm()`. **All in PR #12** (`pi5-support-v2` head `d09373f`), byte-identical to the hw-validated `rcio_source/`.
- RCIO pwmchip index is **not fixed**: `pwmchip6` on kernel 6.6, `pwmchip1` on kernel 6.12 (RP1 enumeration shifts) — ArduPilot detects it at runtime (PR #33655), so never hardcode it.
- Separate device tree overlays: `rcio-overlay.dts` (Pi 4, bcm2709) and `rcio-pi5-overlay.dts` (Pi 5, bcm2712)
- Navio2 RGB LED: device tree overlay `navio2-led.dtbo` creates `/sys/class/leds/rgb_led{0,1,2}` from GPIO4/6/27
- Navio2 compass: `navio2-spi0-cs2.dtbo` adds a 3rd SPI0 chip-select on GPIO22 → `/dev/spidev0.2` → LSM9DS1 magnetometer + AK8963 both detected as compasses. Pi 5 SPI0 exposes only 2 CS by default; without it, enabling the compass panics on the missing `/dev/spidev0.2`.
- Pi 5 RCIO SPI1 uses RP1 `/axi/pcie@120000/rp1/spi@54000` with `dw_spi_mmio` / `spi_dw`; Pi 4 known-good uses BCM SoC SPI at `/soc/...spi@7e215080`
- Pi 3 second-HAT known-good uses BCM aux SPI at `/soc/spi@7e215080`, `spi1.0`, `alive=1`, `board_name=navio2`, `crc=0xb9064332`, git hash `7851d1a`, `pwm_ok=1`
- **No Pi-controlled NRST or BOOT0 pins on Navio2 HAT** — NRST hardwired HIGH (pull-up to 3.3V), BOOT0 tied to GND

## Boat motor reverse — ROOT-CAUSED & FIXED (2026-07-19)

- **Symptom**: Pi 5 boat drove forward only at a forward-only trim (~1100); at a bidirectional 1500-neutral trim the ESC just beeped its power-up jingle **repeatedly** ("kept reconfiguring") and never armed — no forward, no reverse. Not a "reverse" bug; an ESC-never-arms bug.
- **Root cause (RCIO driver)**: `rcio_pwm_update()` only pushed PWM to the STM32 while `armed` was true — a 100 ms watchdog reset **only when ArduPilot writes a PWM value**. ArduPilot writes on *change* only, so any steady value (held stick / disarmed neutral) let the watchdog expire; the driver then stopped refreshing the STM32, which fell back to its **failsafe output (~min)**, not the commanded value. On a bidirectional ESC the signal flickered 1500↔failsafe-min → the ESC re-ran its init forever and never locked onto neutral. A forward-only ~1100 trim ≈ failsafe, so it masked the bug (forward worked). The Pi 3 keeps the STM32 fed → works.
- **How it was found**: instrumented the driver's `armed` flag — it went **false and stayed false** the whole time the throttle was steady, and the code only refreshes the STM32 when `armed`. Confirmed by ESC behaviour after the fix.
- **Fix**: `rcio_source/src/rcio_pwm.c` — push the current values to the STM32 every cycle once the host has started driving (`armtimeout > 0`), instead of gating on the `armed` watchdog. In rcio-dkms **PR #12** (`pi5-support-v2`). Validated on Pi 5 / kernel 6.12.93 with a real bidirectional ESC: init plays once, arms at neutral, **forward + reverse both work**.
- **Diagnostic lesson**: the PWM *value* (sysfs `duty_cycle`) looked perfect (1500, 50 Hz) the whole time — the bug was the driver not *refreshing* the STM32, invisible in sysfs. The user's "the ESC keeps reconfiguring" observation was the key clue (repeated init = repeated signal dropout).
- **Follow-up (future work)**: the fix drops the old ArduPilot-write watchdog; a proper host-heartbeat should replace it (see `docs/TODO.md`). Also note: cheap ESCs re-learn neutral from the signal at power-up, and per-unit neutral varies (may need per-channel `SERVOx_TRIM` tweak).
- **See**: `docs/SESSION_HISTORY.md` Sessions 17 + 19.

## Power module + Hailo HAT power budget (plan finalized 2026-07-19)

- **Problem**: Pi 5 + Navio2 + Hailo HAT together peak ~6–8 A at 5 V. The Navio2 POWER port is a 6-pin JST-PA 2.0mm **analog** port whose BEC is only ~3 A, and its analog V/I sensing is low-precision. CAN/I2C power modules (e.g. PM02 V3.2) do NOT work on the analog POWER connector.
- **Key insight**: **supply and measurement are two separate jobs** — don't make one weak analog PM do both. Solve each with the right part.
- **Plan (parts being selected):**
  1. **Supply** — high-current 5 V DC/DC buck for the Pi 5 stack. Budget ~6–8 A peak at 5 V (Pi 5 ~2.5–3 A, Hailo-8L ~0.5–1 A, NVMe M.2 SSD ~0.5–1.5 A, Navio2 ~0.3 A) → **size to ≥8–10 A continuous (~50 W)**; a 6 A module is marginal. Use a **wide-input buck (e.g. 8–36 V → 5 V, 10 A)** — a buck accepts an input *range*, so a LiPo sagging 12.6→11.1 V (or a 4S pack) is fine; it does NOT need a stable input. Prefer **fixed 5 V** output (no drifting trimpot to over-volt the Pi 5) and verify the **continuous** (not peak) current rating.
  2. **Measure** — motors are **2× Roxxy 3536/06** (~550 W, 50 A max each) → **up to ~100 A peak** at 3S. So a 10 A INA226/228 breakout is far too small, and the Holybro PM02D (30 A continuous wiring) is under-sized too. Use a **hall-effect sensor ≥100 A — Mauch HS-200** (precise, temperature-compensated, no shunt heat; analog V/I → Navio2 analog POWER port; ArduPilot analog battery monitor, calibrate `BATT_AMP_PERVLT`), or **INA228 + a 100–200 A external shunt** if staying I2C (`BATT_MONITOR=21`). Sensor on the main battery + line → reads total pack draw.
- **PM02D BEC = 3 A** → measurement-class only; it cannot feed the Pi 5 + Hailo stack. The 5 V supply is always the separate DC/DC buck.
- **Current boat state**: Hailo HAT removed; Pi 5 + Navio2 + sensors only needs ~2 A, so the existing 3 A analog PM is sufficient *for now*. The Pololu-supply + INA2xx-measurement plan is for when the Hailo HAT is re-installed.
- **See**: `docs/SESSION_HISTORY.md` Session 18 (investigation) + Session 20 (this plan); `docs/TODO.md` task #25.

## Key Conventions

- C++ standard: C++11 (`-std=c++11`)
- Target: aarch64 only (Pi 5 native compilation)
- Sensor status: MPU9250 IMU + MS5611 baro + GPS + 2 compasses (LSM9DS1 magnetometer on spidev0.2 + AK8963 via MPU9250, `navio2-spi0-cs2` 3-CS overlay) — all working & calibrated. LSM9DS1 accel/gyro was tested as a **2nd IMU** (2026-07-05): the chip is healthy (raw SPI reads give real ~1g data on spidev0.3, all speeds), but ArduPilot's LSM9DS1 IMU backend never reads it at runtime on Pi 5 / RP1 — its periodic read callback never fires, so the instance streams zeros and, with `INS_ENABLE_MASK=3`, stalls gyro-cal at boot. **Deferred to future work**; reverted to 1 IMU. See docs/TODO.md.
- Remote target: `ssh pi@<PI5_IP>` (replace with your Pi 5 IP)

## Current Phase

- Phase 1-3: Navio2 driver port COMPLETE
- Phase 4: Hailo AI + Camera + ROS2 integration IN PROGRESS
- Phase 5: ArduPilot Rover on Pi 5 — **COMPLETE** (GPS 3D lock, IMU calibrated, PWM/ADC/RCIO working, RGB LED, systemd service)
- Next step: end-to-end boat test with arming; external F9P GPS and Kogger depth sensor integration; clean up Pi 5 home directory; PRs open for review (emlid/rcio-dkms#11, #12, ArduPilot/ardupilot#33655, #33656). #33647/#33648 closed/superseded.

## Communication Rules

1. **No post-hoc rationalization**: When asked "why did you do X?", state what you actually thought at the time — not a reconstructed plausible answer.
2. **No telling the user what they want to hear**: Answer honestly, even if the answer makes you look wrong or incompetent. Do not be condescending, patronizing, obliging, or accommodating.
3. **If you give a different method than the one you used, say why transparently**: "I used `ps` in my script because X, but suggested `top` to you because Y" — not hiding inconsistency.

## Diagnostic Rules

1. **Check parameters/config before hardware.** When a sensor is "missing" or "Not installed" in QGC, check enable flags, bus assignment, and type params in the parm file (`~/ardurover_work/boat_navio2.parm`) before investigating hardware, SPI, driver, or rebuild theories. The parm file is the highest-probability, lowest-cost check.
2. **Stop when your own docs contradict your theory.** If you find prior documentation (SESSION_HISTORY, TODO, QUICK_START) that contradicts your current diagnosis, stop the theory immediately — don't keep going. That contradiction is the signal that you're on the wrong path.
3. **No post-hoc rationalization applies mid-investigation too.** If you catch yourself constructing a narrative to justify a theory you're already committed to, step back. Apply this rule to yourself during the investigation, not only when answering "why did you do X?" after the fact.
4. **Exhaust simple checks before invasive ones.** A one-line param edit comes before a hwdef edit + 6-8 minute rebuild. A `cat` of the parm file comes before an SPI speed analysis. Never propose an invasive change (rebuild, hwdef edit, driver patch) while a simple check remains untried.

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
