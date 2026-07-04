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

| Sub-task | Status | Details |
|---|---|---|
| 10a. RCIO PR #11 (bugfixes) | ✅ Done | Reviewed, EXPORT_SYMBOL fixed, pushed |
| 10b. RCIO PR #12 (Pi 5 support) | ✅ Done | Dynamic GPIO base (-1), module_param CS delays, pushed |
| 10c. ArduPilot PR #33647 (Linux bugfixes) | ✅ Validated & pushed | PWM_Sysfs retry + INS NONE backend. Guard: `#if CONFIG_HAL_BOARD == HAL_BOARD_ESP32 \|\| AP_INERTIALSENSOR_ALLOW_NO_SENSORS`. Built and tested on Pi 5. |
| 10d. ArduPilot PR #33648 (Navio2 Pi 5) | ⚠️ **Needs guards** | Created but code is NOT guarded — see conflict table below |
| 10e. Forum posts | ❌ Not started | Emlid community + ArduPilot Discourse |

**10d — ArduPilot PR #33648 conflict analysis (must fix before review):**

| File | Change | Pi 4 impact | Issue | Fix needed |
|---|---|---|---|---|
| `boards.py` | `toolchain = 'native'` | Breaks Pi 4 cross-compilation | Pi 4 users may cross-compile with arm-linux-gnueabihf | Add note in PR or make configurable |
| `HAL_Linux_Class.cpp` | `RCOutput_Sysfs(6, ...)` | Breaks Pi 4 (uses pwmchip0) | Hardcoded pwmchip index | Guard with `#if` or make configurable |
| `board/linux.h` | `HAL_BARO_MS5611_I2C_BUS 1` | May affect other Linux boards | Unguarded define | Guard with `#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO2` |
| `AP_Baro_MS5611.cpp` | Skip PROM CRC check | Affects all MS5611 users | Not Navio2-specific | Guard with `#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO2` |
| `AP_InertialSensor_config.h` | `ALLOW_NO_SENSORS=1` | Changes behavior for all Linux boards | Not Navio2-specific | Guard or move to board config |
| `AP_InertialSensor.cpp` | Warn instead of panic | Changes behavior for all Linux boards | Not Navio2-specific | Guard with `#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO2` |
| `AP_InertialSensor_NONE.h/cpp` | Enable NONE backend | Safe — opt-in | OK | In PR #33647, validated |
| `PWM_Sysfs.cpp` | Retry loop for duty_cycle fd | Safe — retry with timeout | OK | In PR #33647, validated |

### Pending

| # | Task | Priority | Sub-tasks |
|---|---|---|---|
| 9 | Repair LiPo power connector | High | Physical repair (user) — fire risk before water test |
| 14 | Full QGC calibration | High | Radio calibration, flight modes, failsafe, servo direction |
| 15 | Clean up Pi 5 home directory | Medium | Remove ~80 debug files, old params, stale dirs |
| 11 | Install ROS2 Jazzy + ArduPilot DDS | Medium | Install ROS2, configure AP_DDS, micro-ros-agent |
| 12 | Hailo-8L + ROS2 inference pipeline | Medium | Semantic segmentation model, ROS2 camera topic, Hailo inference |
| 13 | Evaluate Navigator (BlueRobotics) | Low | Research only — Pi 4 only, unknown Pi 5 porting feasibility |

## Next Steps (immediate priority)

1. **Fix PR #33648** — add `#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO2` guards to 4 files, handle pwmchip index and toolchain for Pi 4 compatibility. Validate on Pi 5 before pushing.
2. **Forum posts** — Emlid community + ArduPilot Discourse.
3. **Repair LiPo power connector** (user task).
4. **Full QGC calibration** — radio, flight modes, failsafe, servo direction.
5. **Clean up Pi 5 home directory**.
6. **Install ROS2 Jazzy + ArduPilot DDS**.
7. **Hailo-8L + ROS2 inference pipeline**.

## PR Tracker

| PR | Repo | Status | Notes |
|---|---|---|---|
| [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) | emlid/rcio-dkms | Open | Bugfixes — safe for all platforms, ready for review |
| [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) | emlid/rcio-dkms | Open | Pi 5 support — dynamic GPIO, module_param, ready for review |
| [ArduPilot/ardupilot#33647](https://github.com/ArduPilot/ardupilot/pull/33647) | ArduPilot/ardupilot | Open | Linux bugfixes — validated on Pi 5, ready for review |
| [ArduPilot/ardupilot#33648](https://github.com/ArduPilot/ardupilot/pull/33648) | ArduPilot/ardupilot | Open | Navio2 Pi 5 — **needs guards added before review** |
| [axonbf/navio2-rpi5-ardupilot](https://github.com/axonbf/navio2-rpi5-ardupilot) | Public repo | Live | Setup guide, scripts, overlays, docs |
| ~~emlid/rcio-dkms#10~~ | emlid/rcio-dkms | Closed | Replaced by #11 + #12 |
| ~~ArduPilot/ardupilot#33645~~ | ArduPilot/ardupilot | Closed | Replaced by #33647 + #33648 |

## Notes for next session (Claude CLI)

### Current state — READ THIS FIRST

The PRs have been restructured from the original single PR into multiple PRs. Do NOT start from scratch. The current structure is:

**RCIO (emlid/rcio-dkms):**
- PR #11 (bugfixes): DONE, reviewed, validated, pushed. Branch: `bugfixes` on `axonbf/rcio-dkms`
- PR #12 (Pi 5 support): DONE, reviewed, validated, pushed. Branch: `pi5-support-v2` on `axonbf/rcio-dkms`

**ArduPilot (ArduPilot/ardupilot):**
- PR #33647 (Linux bugfixes): DONE, validated on Pi 5, pushed. Branch: `linux-bugfixes` on `axonbf/ardupilot`
  - Commit 1: `AP_HAL_Linux: PWM_Sysfs: add retry loop for duty_cycle fd open`
  - Commit 2: `AP_InertialSensor: enable NONE backend when ALLOW_NO_SENSORS is set`
  - Guard: `#if CONFIG_HAL_BOARD == HAL_BOARD_ESP32 || AP_INERTIALSENSOR_ALLOW_NO_SENSORS`
  - Built and tested on Pi 5 — boots successfully
- PR #33648 (Navio2 Pi 5): CREATED but **NOT GUARDED** — this is the immediate next task. Branch: `navio2-pi5-support` on `axonbf/ardupilot`
  - 4 commits, 6 files need Pi 4 compatibility guards
  - See conflict table above

### How to get the working branches

The `/tmp/` directories from the previous session will NOT persist. Clone from the forks:

```bash
# ArduPilot fork
git clone https://github.com/axonbf/ardupilot.git /tmp/ardupilot_navio2_pi5
cd /tmp/ardupilot_navio2_pi5
git fetch origin tag Rover-4.6.3 --depth=1
git checkout linux-bugfixes    # PR #33647 (done)
git checkout navio2-pi5-support # PR #33648 (needs guards)

# RCIO fork
git clone https://github.com/axonbf/rcio-dkms.git /tmp/rcio_navio2_pi5
cd /tmp/rcio_navio2_pi5
git checkout bugfixes       # PR #11 (done)
git checkout pi5-support-v2 # PR #12 (done)
```

### Pi 5 state

The Pi 5 (`~/ardupilot/`) has a mix of old and new changes — some files were updated during validation, others are still the original unguarded versions. **Do NOT use the Pi 5 source as the source of truth for PRs.** Always work from the GitHub fork branches.

The Pi 5's `~/ardupilot/libraries/AP_InertialSensor/AP_InertialSensor_NONE.h` and `.cpp` were updated and validated (PR #33647 changes). The other files on the Pi 5 are the old unguarded versions.

### Rules

- **Always validate before committing** — build on Pi 5 (`./waf build -j4`, ~15 min), test if runtime behavior changes.
- **Always ask before applying changes to the Pi 5.** Do not push files, modify source, or rebuild without explicit user approval.
- **Keep docs up-to-date** before moving to the next task.
- **Communication rules** in `AGENTS.md`: no post-hoc rationalization, no telling user what they want to hear, transparent about method differences.
- **PR #33648 is the immediate next task** — 6 files need Pi 4 compatibility guards.

### Access info

- **SSH access**: `sshpass -p 'raspberry' ssh pi@192.168.178.42`
- **Start ArduPilot**: `sudo ardurover --serial1 udp:192.168.178.20:14550 --defaults ~/ardurover_work/boat_navio2.parm`
- **Stop ArduPilot**: `sudo systemctl stop ardurover` or `sudo killall ardurover`
- **Build**: `cd ~/ardupilot && ./waf build -j4` (~15 min, use long timeouts)
- **CPU measurement**: use `ps -C ardurover` or `pgrep -x ardurover`, NOT `ps -o pcpu= -p $!` (reads sudo wrapper, gives 0%)
- **GitHub**: authenticated as `axonbf` (personal account)