# TODO — Navio2 + Pi 5 Project

## Document Status

`current`

## High-Level TODOs

### Completed

| # | Task | Details |
|---|---|---|
| 1 | Resolve I2C + RCIO bus contention | MS5611 works concurrently with RCIO — no sequencing needed |
| 2 | Rebuild ArduPilot for navio2 board | `build/navio2/bin/ardurover` confirmed working |
| 3 | Test PWM output via RCIO | 14 channels exported, 1500ns neutral confirmed |
| 4 | Add GPS support (M8N on SPI) | 3D lock, 11+ satellites, GPS_TYPE=2 |
| 5 | Create systemd service | `/etc/systemd/system/ardurover.service` + `/etc/default/ardurover` |
| 6 | End-to-end boat test | Motors respond to RC, all sensors working |
| 7 | Fix Navio2 RGB LED | Dynamic GPIO base (-1) + `navio2-led.dtbo` overlay |
| 8 | Update docs | Architecture diagrams, QUICK_START, TECHNICAL_DESIGN, all synced |

### In Progress

#### 10 — Publish: ArduPilot PRs + RCIO PRs + forum posts

**Resolved 2026-07-05 (supersedes the old #33647/#33648 plan below).** Re-analysis against current master (hwdef system) dropped most changes; only C + G survive, each as a clean PR against master. Full detail in the A–G issue map further down.

| Sub-task | Status | Details |
|---|---|---|
| 10a. RCIO PR #11 (bugfixes) | ✅ Open | Reviewed, EXPORT_SYMBOL fixed, pushed |
| 10b. RCIO PR #12 (Pi 5 support) | ✅ Open | Dynamic GPIO base (-1), module_param CS delays, pushed |
| 10c. ArduPilot PR #33655 (change C — pwmchip) | ✅ Open | Runtime pwmchip detection, against master, HW-tested, no AI trailer |
| 10d. ArduPilot PR #33656 (change G — PWM_Sysfs) | ✅ Open | duty_cycle retry on slow export, against master, HW-tested |
| 10e. Forum posts | ❌ Not started | Emlid community + ArduPilot Discourse |

The earlier **#33647 + #33648** drafts were **closed** — unguarded, based on the 4.6.3 tag, and folded the now-dropped changes (A/B/D/E/F). Their old per-file conflict analysis no longer applies: A/B (config in hwdef), D (CRC valid on HW), E/F (dormant, MPU9250 works) were all dropped, and C is now runtime-detected (no guard needed). See the A–G issue map for the disposition of each.

### Revised issue map — updated 2026-07-05 (current master uses hwdef for Linux boards)

Investigating against current ArduPilot master (`48afb18`) changed the picture: master migrated Linux boards to a **hwdef** system (`libraries/AP_HAL_Linux/hwdef/navio2/hwdef.dat`), which already solves two of the original changes.

Final change map uses letters **A–G** (avoids collision with the old #1–#5). Both closed PRs (#33648 + #33647) folded in.

| ID | (was) | Change | File(s) | Disposition | Validated | Approved |
|----|-------|--------|---------|-------------|-----------|----------|
| **A** | #1 | `toolchain='native'` | `boards.py` | **No code change** — 64-bit Pi builds pass `--toolchain=native` at configure (NOT obsolete — see correction below; document in QUICK_START) | ☑ master native build | ☑ |
| **B** | #2 | `HAL_BARO_MS5611_I2C_BUS 1` | `AP_HAL/board/linux.h` | Drop — hwdef declares `BARO MS5611 I2C:1:0x77` | ☑ master: MS5611 on bus 1 | ☑ |
| **C** | #3 | pwmchip index → runtime detect | `AP_HAL_Linux/HAL_Linux_Class.cpp` | **Keep → PR-1** | ☑ master: pwmchip6, 14 ch @ 1500 µs | ☑ |
| **D** | #4 | skip MS5611 PROM CRC | `AP_Baro/AP_Baro_MS5611.cpp` | Drop — CRC valid on HW | ☑ master: CRC enforced, baro found | ☑ |
| **E** | #5 | allow-no-sensors + warn | `AP_InertialSensor.cpp`, `_config.h` | Drop — dormant + unsafe global flip | ☑ boots on MPU9250 | ☑ |
| **F** | — | enable NONE dummy-IMU backend | `AP_InertialSensor_NONE.*` | Drop — inert without E | via E | ☑ |
| **G** | — | PWM_Sysfs `duty_cycle` retry loop | `AP_HAL_Linux/PWM_Sysfs.cpp` | **Keep → PR-2** | ☑ master: compiles + PWM works | ☑ |

**Master build validation (2026-07-05):** cloned ArduPilot master (`5152cde`) → `~/ardupilot-master`, applied C+G, `./waf configure --board navio2 --toolchain=native` + `./waf rover` (exit 0), boot-tested the master binary (`ArduRover V4.8.0-dev`): `MS5611 found on bus 1` (B, D), **pwmchip6 with 14 channels @ 1500 µs** (C), MPU9250 normal boot (E). `~/ardupilot` (4.6.3) left untouched; boat down.

**A correction (important):** the earlier "obsolete" verdict was **wrong**. Master's navio2 board still defaults to the `arm-linux-gnueabihf` (32-bit) toolchain, so a 64-bit Pi build fails unless you pass **`--toolchain=native`** at configure. It's a build flag, not a code change — so A stays out of the PR — but **QUICK_START Step 8 must be updated** to add `--toolchain=native` and to drop the now-removed #4/#5 (D/E) patch instructions.

**PRs open (2026-07-05):** [ArduPilot/ardupilot#33655](https://github.com/ArduPilot/ardupilot/pull/33655) (C, pwmchip runtime detection) and [#33656](https://github.com/ArduPilot/ardupilot/pull/33656) (G, PWM_Sysfs retry) — clean, template-formatted, hardware-tested, no AI trailer. RCIO PRs [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) + [#12](https://github.com/emlid/rcio-dkms/pull/12) still open (clean/mergeable; #12 stacked on #11). **Update 2026-07-12:** #12 extended with kernel 6.12 support + CRC fix + pwm cleanup (head `f3a4887`) — see the kernel-6.12 note below.

**Compass resolved (2026-07-05):** Pi 5's SPI0 lacked `/dev/spidev0.2` (RP1 exposes only 2 CS), so enabling the compass **panicked**. Added `navio2-spi0-cs2` overlay (3rd CS on GPIO22) → `spidev0.2` appears → **2 compasses detected** (LSM9DS1 magnetometer devtype 6 on spidev0.2 + AK8963 devtype 4 via MPU9250). Compass enabled in `boat_navio2.parm` (backup `.pre-compass.bak`); **calibration completed successfully by user.** QUICK_START/AGENTS/overlay source updated. **Follow-up ANSWERED (see 2nd-IMU note below):** the LSM9DS1 accel/gyro on GPIO25 is **not** defective — the chip is healthy; the blocker is ArduPilot's LSM9DS1 IMU driver on Pi 5 / RP1, not hardware.

**#5 validation (2026-07-05):** reverted *only* #5 on the Pi's 4.6.3 tree (backups saved as `.pre5.bak`), rebuilt ArduRover (13m46s), boot-tested. Result: boots identically on the real MPU9250 — no panic, no NONE backend, no `unable to initialise`, MS5611 found. Confirmed the running binary was the freshly-built one (mtime matches build end; both #5-only strings absent from the binary via `strings`). Conclusion: #5's code was dormant (MPU9250 works); the broken **LSM9DS1** is skipped by ArduPilot's baseline runtime probing (`WHO_AM_I=0xFF`), not by #5. **#33648 will not include #5.** Boat left stopped after the test; #5 kept reverted on the Pi.

**#3 validation (2026-07-05):** replaced the hard-coded pwmchip index with a runtime helper `navio2_rcio_pwmchip()` in `HAL_Linux_Class.cpp` (backup `.pre3.bak`). It scans `/sys/class/pwm/pwmchip*/npwm` and picks the RCIO chip by its 14-channel count — `pwmchip0` on Pi 4 (BCM), `pwmchip6` on Pi 5 (RP1 shifts enumeration). Chosen over `UtilRPI::detect_linux_board_type()` because `rcoutDriver` is a static global built before `hal.util` is ready. Rebuilt (compiles clean, exit 0) and boot-tested on the Pi 5: helper selected **pwmchip6**, all **14 channels exported** (pwm0–pwm13) at **50 Hz** (period 20 ms) with **1500 µs** neutral on active outputs (pwm0/pwm2); `pwmchip0`/`pwmchip2` untouched. Pi 4 path (chip 0) correct by construction, not hardware-tested. Boat left down.

**#4 investigation (2026-07-05, read-only — no change applied):** with the boat stopped, read the MS5611 PROM directly over `i2c-1` (0x77) and computed the MS5611 CRC4. C1–C6 are valid calibration values; stored `crc_read=0xF` equals computed CRC `0xF` → **the barometer's CRC is valid on this hardware.** The CRC-skip (#4) is therefore unnecessary — most likely a stale workaround from before the I²C/RCIO bus contention was resolved (task 1). **Recommendation: drop #4**, restoring upstream CRC enforcement. Definitive confirmation pending user go-ahead: revert #4 on the Pi, rebuild, and verify `MS5611 found on bus 1 address 0x77` still appears at ArduPilot init. **Confirmed:** reverted #4 on the Pi (backup `.pre4.bak`), rebuilt, boot-tested with CRC enforced → `MS5611 found on bus 1 address 0x77` still appears, so the CRC passes at ArduPilot init. #4 dropped.

**LSM9DS1 as 2nd IMU — investigated 2026-07-05, DEFERRED to future work.** Goal was a 2nd IMU for redundancy using the LSM9DS1 accel/gyro (spidev0.3, GPIO25).

- **Chip is healthy.** Raw `spidev` reads of the accel output registers (0x28–0x2D) return valid ~1 g gravity data at **every** SPI speed 1–10 MHz. WHO_AM_I = 0x68. Not a hardware defect.
- **ArduPilot's LSM9DS1 IMU backend never reads it at runtime on Pi 5 / RP1.** The instance registers (`INS_ACC2_ID` set) but its periodic read callback **never fires** (0 reads in 26 s), so it streams flat zeros. Worse, with `INS_ENABLE_MASK=3` the phantom instance **stalls gyro-cal at boot** (`Init Gyro***` forever).
- **Tried, none produced data:** (1) `INS_ENABLE_MASK` gating — found it was blocking the probe entirely, set to 3; (2) FIFO-bypass + direct-register reads in `_poll_data`; (3) SPI speed 1–10 MHz; (4) disabling the LSM9DS1 compass (ruled out mag/AG chip contention); (5) swapping `_dev->transfer()` → `_dev->read_registers()` (the exact call the working MPU9250 uses). Instrumented the read path with `printf` — confirmed the read function is never called.
- **Root cause:** ArduPilot's LSM9DS1 INS backend lifecycle on this platform (callback never registers/executes), not the chip, SPI, speed, FIFO, or config writes.
- **Reverted** to clean 1 IMU (MPU9250) + 2 compasses (AK8963 + LSM9DS1 mag). On-Pi backups: `hwdef.dat.pre-imu.bak`, `*.dtbo.4cs.bak`. `hwdef.dat` + driver restored to pristine; only the 2 PR keepers (C in `HAL_Linux_Class.cpp`, G in `PWM_Sysfs.cpp`) remain modified. Verified: 1 IMU + 2 compass, boots fully.
- **Note (build gotcha):** on Linux boards `hwdef.dat` is processed at **`./waf configure`** time, not `./waf rover` — a build-only run silently reuses the stale generated `hwdef.h`. Always reconfigure after editing `hwdef.dat`.

### Pending

| # | Task | Priority | Sub-tasks |
|---|---|---|---|
| 8 | LSM9DS1 2nd IMU (deferred) | Low | Future work only — see verdict above; needs upstream LSM9DS1 backend debug on Pi 5 / RP1 |
| ~~9~~ | ✅ **DONE** — Repair LiPo power connector | High | Fixed 2026-07-12 |
| ~~14~~ | ✅ **Done/deferred** — RC-only, no QGC needed | High | 2026-07-19 (user): boat flown via RC without QGC; radio/modes configured. Only battery config in QGC remains, and it's optional while RC-only → deferred. |
| ~~15~~ | ✅ **N/A** — was for the OLD 6.6 card (retired) | Low | Verified 2026-07-19 (Claude): the **current card is the fresh 6.12 reproduction** — home dir is clean (30 std items, 0 debug files). The ~80-debug-files cleanup applied to the old dev card, now retired. Moot. |
| 11 | Install ROS2 **Humble** + ArduPilot native DDS | Medium | **Plan → [`docs/ROS2_HUMBLE_DDS_PLAN.md`](ROS2_HUMBLE_DDS_PLAN.md)** (6 steps). **Steps 1 & 4 DONE 2026-08-01** — ROS 2 **Humble** installed & verified on the Pi via RoboStack (isolated conda env `ros_env`, batch install, `.bashrc` untouched, DDS pub/sub confirmed); **CycloneDDS-over-WireGuard** configured & verified (`~/cyclonedds.xml` wg0 unicast 10.0.0.5↔10.0.0.2, helper `~/ros2_env.sh`). **Distro = Humble not Jazzy**: RoboStack has no Jazzy for aarch64, only Humble — which also matches the laptop's native Humble (cleanest interop). Vehicle = RoboStack (not Docker — NAT breaks DDS discovery over WireGuard); integration = native DDS (AP_DDS) + micro-ROS agent, not MAVROS. **Remaining:** Step 2 (rebuild `ardurover --enable-dds`) — **AFTER the lake**; then Step 3 (micro-ROS agent — not in RoboStack, source-build the version matching ArduPilot's XRCE-DDS client, hence deferred until after Step 2), Step 5 (laptop: add rmw_cyclonedds + same config), Step 6 (verify over WG). |
| 12 | Hailo-8L + ROS2 inference pipeline | Medium | Semantic segmentation model, ROS2 camera topic, Hailo inference |
| 13 | Evaluate Navigator (BlueRobotics) | Low | Research only — Pi 4 only, unknown Pi 5 porting feasibility |
| ~~17~~ | ✅ **DONE** — RCIO PR #12 updated with kernel 6.12 fixes + CRC fix + pwm cleanup | Medium | Pushed to `pi5-support-v2` (head `f3a4887`): 6.12 API guards (`pwmchip_alloc`/`gpiochip_add_data`/`pwm_ops.owner`), CRC sign-extension fix, dead-code removal. Byte-identical to hw-validated `rcio_source/`. |
| ~~18~~ | ✅ **DONE** — Claude audit of the 6.12 fixes | Medium | Verdict: 6.12 port correct & hw-validated (alive=1, pwmchip1 npwm=14 on 6.12.93). Found+fixed: CRC sign-extension (A), dead `to_rcio_pwm` (M1), probe-success-on-failure logging (M2), pwmchip-index doc accuracy (D1). See Session 15 in SESSION_HISTORY. |
| ~~19~~ | ✅ **DONE** — `ardurover` service enabled on clean card | Medium | Service enabled + active on clean 6.12 card (boot auto-start). |
| 20 | **C** — scrub corporate email from public repo | Medium | **opencode** — commit `5b77d93` was authored as `...@agcocorp.com`; re-attribute to `axonbf`/`mail@benjaminfernandez.info`. (Claude already set repo-local git identity so future commits are correct.) |
| 21 | **M2b** — unwind pwmchip on probe failure (optional hardening) | Low | Claude/opencode — `rcio_pwm_probe` still leaves the added pwmchip registered if `rcio_hardware_init()` fails; low-risk error path, not triggerable on working HW. Only the false "success" log was fixed (M2). |
| ~~22~~ | ✅ **DONE** — Boat reverse fixed (RCIO driver, not ArduPilot/ESC) | High | **Root cause was NOT a 4.8 param regression** — `rcio_pwm_update()` stopped refreshing the STM32 on steady values (`armed` watchdog expired), so the STM32 fell back to failsafe-min and bidirectional ESCs never locked onto 1500 neutral. Fix: refresh continuously once driving (`armtimeout>0`). Repo `ba1d62a` + rcio-dkms PR #12 (`f3a4887`). HW-validated: forward + reverse both work. See SESSION_HISTORY Session 19 + AGENTS "Boat motor reverse". |
| ~~27~~ | ✅ **Decided** — T200 rejected (too expensive) → treat the Roxxy | Medium | 2026-07-19 (user): T200 too pricey (~€400/pair, wants 4S). Path = **treat existing Roxxy for freshwater submersion** (stainless/ceramic bearings + potted/coated windings), reusing Roxxy + QuicRun. Physical treatment is the remaining bench work; decision closed. |
| 28 | RPM measurement path (Navio2 has no DShot) | Low | Navio2/Navigator = servo PWM only, no bidir-DShot eRPM. For submerged thrusters, RPM must come from ESC eRPM via serial telemetry (needs a telemetry-capable ESC → Pi UART → ArduPilot ESC telemetry). Physical optical/Hall sensors only work with a dry accessible shaft. See AGENTS "Propulsion & RPM". |
| 29 | Install RCIO module via **DKMS** (auto-rebuild on kernel upgrades) | Medium | **Why:** RCIO `.ko` is hand-built for one kernel (6.12.93) and NOT DKMS-managed, so any `apt` kernel upgrade breaks it (won't load → no PWM/ADC/RC) until manually rebuilt — verified 2026-07-31: a `6.12.96` kernel upgrade is already queued. **What:** register `rcio_source/` (ships `dkms.conf`) with DKMS (`dkms add/build/install`) so the kernel auto-rebuilds it on every kernel update. **Trigger:** do this before enabling routine `apt upgrade`; then the boat survives kernel updates unattended. (Claude, 2026-07-31) |
| 26 | **Host-heartbeat for RCIO PWM** (future work) | Medium | The reverse fix drops the old ArduPilot-write watchdog (feeds STM32 continuously once `armtimeout>0`). Add a proper host-heartbeat so outputs failsafe if the ArduPilot *process* dies while the kernel worker lives (whole-Pi death is already covered by the STM32's own timeout). See `rcio_source/src/rcio_pwm.c` `rcio_pwm_update()`. |
| ~~23~~ | ✅ **Not a bug** — RC runs via RCIO, not serial2 | Low | Verified 2026-07-19 (Claude): RC input comes through the **Navio2 RC-input (RCIO `rcin`)** — live channels confirmed (throttle ch2). `--serial2 /dev/ttyAMA0` points to a non-existent device so ardurover just skips serial2 (harmless; RC unaffected). opencode's assumption that the H12 was on serial2 was wrong. Only change it to `/dev/ttyAMA10` if you later want serial2 for a real UART device (2nd telemetry / serial RC). |
| ~~24~~ | ✅ **Resolved** — port split is intentional | Low | 2026-07-19 (user): **Pi 3 = 14550, Pi 5 = 14551**, chosen deliberately to avoid collision when both run. Not a bug. |
| 25 | **Power: high-current supply + high-current precise measurement** (DC/DC supply bought 2026-07-19; measurement sensor TBD) | Medium | Plan refined 2026-07-19 (supersedes the old "verify analog PM" item). **Supply:** ✅ **bought** — wide-input buck **8–36 V → 5 V, ≥8–10 A continuous (~50 W)** for Pi 5 + Hailo + NVMe SSD stack (~6–8 A peak; 6 A is marginal). Fixed 5 V out preferred (no pot drift); a buck takes an input *range* so a sagging LiPo is fine. **Measure:** motors 2× Roxxy 3536/06 → **~100 A peak** at 3S, so 10 A INA breakouts AND the Holybro PM02D (30 A wiring) are too small → use **Mauch HS-200 hall-effect** (analog → Navio2 analog POWER port, ArduPilot analog batt monitor, calibrate `BATT_AMP_PERVLT`) or **INA228 + 100–200 A external shunt** (I2C, `BATT_MONITOR=21`); on the main battery + line = total draw. PM02D's 3 A BEC is measurement-class only. Validate on HW when parts arrive; only needed once Hailo HAT is re-installed (now removed → 3 A analog PM suffices). See AGENTS "Power module" + SESSION_HISTORY Session 20. |

## Next Steps (immediate priority)

1. ~~Boat reverse regression (task #22)~~ — ✅ **DONE**, root-caused to the RCIO PWM-refresh bug and fixed (repo `ba1d62a`, rcio-dkms PR #12). Reverse works.
2. **Await review on PRs #33655 (C) + #33656 (G)** — both open, clean, hardware-tested. The old unguarded #33647/#33648 were closed and superseded; no further guard work needed.
3. **Forum posts** — Emlid community + ArduPilot Discourse.
4. **Full QGC calibration** — radio, flight modes, failsafe, servo direction.
5. **Fix `--serial2 /dev/ttyAMA0` → `/dev/ttyAMA10`** in `/etc/default/ardurover` when RC receiver is connected to Pi 5 (task #23).
6. **Clean up Pi 5 home directory**.
7. **Install ROS2 Jazzy + ArduPilot DDS**.
8. **Hailo-8L + ROS2 inference pipeline**.

## PR Tracker

| PR | Repo | Status | Notes |
|---|---|---|---|
| [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) | emlid/rcio-dkms | Open | Bugfixes — safe for all platforms, ready for review |
| [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) | emlid/rcio-dkms | Open | Pi 5 support — dynamic GPIO, module_param, **+ kernel 6.12 support + CRC fix** (head `f3a4887`, 2026-07-12) |
| [ArduPilot/ardupilot#33655](https://github.com/ArduPilot/ardupilot/pull/33655) | ArduPilot/ardupilot | **Open** | Change C — Navio2 RCIO pwmchip runtime detection. Clean, template-formatted, hardware-tested, no AI trailer |
| [ArduPilot/ardupilot#33656](https://github.com/ArduPilot/ardupilot/pull/33656) | ArduPilot/ardupilot | **Open** | Change G — PWM_Sysfs duty_cycle retry on slow export. Clean, template-formatted, hardware-tested |
| [axonbf/navio2-rpi5-ardupilot](https://github.com/axonbf/navio2-rpi5-ardupilot) | Public repo | Live | Setup guide, scripts, overlays, docs |
| ~~emlid/rcio-dkms#10~~ | emlid/rcio-dkms | Closed | Replaced by #11 + #12 |
| ~~ArduPilot/ardupilot#33645~~ | ArduPilot/ardupilot | Closed | Replaced by earlier PRs |
| ~~ArduPilot/ardupilot#33647~~ | ArduPilot/ardupilot | Closed | Unguarded "AI slop" draft — superseded by #33656 (G) |
| ~~ArduPilot/ardupilot#33648~~ | ArduPilot/ardupilot | Closed | Unguarded draft — superseded by #33655 (C) |

## Notes for next session (Claude CLI)

### Current state — READ THIS FIRST (updated 2026-07-05)

**Up to date as of 2026-07-05:** ArduPilot changes are now **two clean PRs — #33655 (C, pwmchip runtime detect) and #33656 (G, PWM_Sysfs retry)** — both OPEN, hardware-tested, awaiting review. The old drafts **#33647 + #33648 are CLOSED/superseded** (see PR Tracker). Work now happens against **ArduPilot master** cloned to `~/ardupilot-master` on the Pi (not the old 4.6.3 `~/ardupilot`). The Pi runs a clean **1 IMU (MPU9250) + 2 compass** build; LSM9DS1 2nd-IMU is deferred (verdict above). The historical branch/clone notes below are kept for reference but are **superseded** by the above.

**RCIO (emlid/rcio-dkms):**
- PR #11 (bugfixes): DONE, reviewed, validated, pushed. Branch: `bugfixes` on `axonbf/rcio-dkms`
- PR #12 (Pi 5 support): DONE, reviewed, validated, pushed. Branch: `pi5-support-v2` on `axonbf/rcio-dkms`

**ArduPilot (ArduPilot/ardupilot) — against master, each a single clean commit:**
- PR #33655 (change C — RCIO pwmchip runtime detect): OPEN, HW-tested. Branch `pr-navio2-pwmchip` on `axonbf/ardupilot` @ `84182ac`.
- PR #33656 (change G — PWM_Sysfs duty_cycle retry): OPEN, HW-tested. Branch `pr-pwm-sysfs-retry` on `axonbf/ardupilot` @ `15967de`.
- Old `#33647`/`#33648` (branches `linux-bugfixes`, `navio2-pi5-support`) are **closed/superseded** — do not use.

### How to get the working branches (pinned)

Sensors need no patch — master's navio2 hwdef declares MPU9250 + LSM9DS1 mag + AK8963. Only the two ArduPilot commits are needed; cherry-pick them by SHA onto a fresh master clone (full recipe in `QUICK_START.md` Step 8):

```bash
# ArduPilot: clone master, add the fork, cherry-pick the two pinned commits
git clone https://github.com/ArduPilot/ardupilot.git ~/ardupilot-master
cd ~/ardupilot-master
git remote add axonbf https://github.com/axonbf/ardupilot.git
git fetch axonbf pr-navio2-pwmchip pr-pwm-sysfs-retry
git cherry-pick 84182acccb82dabbb771afcb30d400bc742282d9   # C — PR #33655
git cherry-pick 15967de4b9fdde3d7b99bfa533e6c22fe701c66e   # G — PR #33656
git submodule update --init --recursive

# RCIO module + overlays are vendored in THIS repo's rcio_source/ (build per QUICK_START Step 4).
# The rcio-dkms fork branches (bugfixes @ e2d2c36, pi5-support-v2 @ f3a4887) are provenance only.
```

### Pi 5 state (updated 2026-07-05)

**Active working tree is `~/ardupilot-master`** (fresh clone of ArduPilot master) — NOT the old `~/ardupilot` (4.6.3), which is kept only for reference; do not build it. Current `~/ardupilot-master`:
- Branch `pr-navio2-pwmchip` (fork `axonbf`), 1 commit ahead of master: `84182ac` (change C, RCIO pwmchip runtime detect = PR #33655).
- `libraries/AP_HAL_Linux/PWM_Sysfs.cpp` is an **uncommitted** working-tree edit (change G = PR #33656; that PR's own branch is `pr-pwm-sysfs-retry` @ `15967de`).
- `hwdef.dat` + the LSM9DS1 driver are pristine/upstream (2nd-IMU revert complete). `INS_ENABLE_MASK=1` → clean **1 IMU (MPU9250) + 2 compass** system, boots fully.
- Leftover `.bak` files in the tree (`hwdef.dat.pre-imu.bak`, `AP_InertialSensor_LSM9DS1.cpp.{dbgbak,fifobak,rrbak}`, `overlays/*.dtbo.4cs.bak`) — safe to delete (cleanup pending, task #15).
- `/usr/bin/ardurover` → symlink to `~/ardupilot-master/build/navio2/bin/ardurover`. Systemd service + `boat_navio2.parm` live in `~/ardurover_work` (service `WorkingDirectory` pinned there so param storage is stable).
- **Build gotcha:** on Linux boards `hwdef.dat` is regenerated at `./waf configure` time, not `./waf rover` — always reconfigure after editing it, or the build silently reuses a stale `hwdef.h`.

**Do NOT treat the Pi source as the source of truth for PRs** — the canonical PR content is the fork branches (`pr-navio2-pwmchip`, `pr-pwm-sysfs-retry`).

### Rules

- **Always validate before committing** — build on Pi 5 (`./waf build -j4`, ~15 min), test if runtime behavior changes.
- **Always ask before applying changes to the Pi 5.** Do not push files, modify source, or rebuild without explicit user approval.
- **Keep docs up-to-date** before moving to the next task.
- **Communication rules** in `AGENTS.md`: no post-hoc rationalization, no telling user what they want to hear, transparent about method differences.
- **No immediate ArduPilot PR task** — #33655 + #33656 are open and awaiting upstream review; nothing to push. Next real work is forum posts / QGC calibration / cleanup (see Pending).

### Access info

- **SSH access**: `sshpass -p 'raspberry' ssh pi@192.168.178.42`
- **Start ArduPilot**: `sudo ardurover --serial1 udp:192.168.178.20:14550 --defaults ~/ardurover_work/boat_navio2.parm`
- **Stop ArduPilot**: `sudo systemctl stop ardurover` or `sudo killall ardurover`
- **Build**: `cd ~/ardupilot-master && ./waf configure --board navio2 --toolchain=native && ./waf rover` (reconfigure required after any `hwdef.dat` edit; ~6–8 min incremental, ~25–40 min from scratch — use long timeouts). The old `~/ardupilot` (4.6.3) is reference-only.
- **CPU measurement**: use `ps -C ardurover` or `pgrep -x ardurover`, NOT `ps -o pcpu= -p $!` (reads sudo wrapper, gives 0%)
- **GitHub**: authenticated as `axonbf` (personal account)