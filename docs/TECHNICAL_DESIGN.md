# Technical Design

## Document Status

`current`

## General Project Definition Rules

1. Define with a clear phrase the problem and functional objective.
2. Delimit initial scope and out of scope to avoid ambiguity in implementation.
3. Establish verifiable success criteria (functional and technical).
4. Declare relevant technical constraints (environment, dependencies, resources, operational limits).
5. Centralize relevant operational configuration of the system and avoid constants or parameters scattered without a common definition.
6. Explicitly model critical states, artifacts or intermediate data of the flow to reduce ambiguity between stages.
7. Identify initial risks and their mitigation strategy.
8. Keep closed decisions separate from pending decisions.
9. When closing the design phase, update the document status and link with setup, architecture and plan.

## Objective

Provide full Navio2 HAT support on Raspberry Pi 5 for current ArduPilot Rover, using Linux and Pi configuration plus targeted ArduPilot compatibility changes where needed.

## Base Use Case

- Starting point: Upstream emlid/Navio2 C++ sources using pigpio for GPIO/PWM/SPI/I2C.
- Current or planned input: Same sensor hardware (MPU9250, LSM9DS1, MS5611, U-blox M8N, RCIO co-processor) on the Navio2 HAT.
- Current or planned output: Identical sensor readings and control signals via same SPI/I2C/PWM interfaces.
- Objective of the new flow: Replace low-level GPIO/SYSFS access with native Pi 5 equivalents while keeping the abstraction layer API unchanged.

Additional ArduPilot-specific rule:
- The target is not only developer examples but also real ArduRover operation with Navio2 peripherals working as ArduPilot expects on a healthy Navio2 HAT.
- The only accepted hardware exception is the known defective LSM9DS1 on the current board.

## Design Questions (resolved)

- **Q: Can we keep pigpio?** No. pigpio relies on /dev/mem and BCM2835/BCM2711-specific peripheral addresses. Pi 5's RP1 chip uses PCIe-attached GPIO controller with different register map. pigpio does not support Pi 5.
- **Q: What replaces pigpio?** libgpiod (gpiod) v2 — the standard userspace interface for /dev/gpiochip on modern kernels. For PWM, we use /sys/class/pwm (pwmchip) which is supported on Pi 5 with the pwm_bcm2835-style overlay. For SPI/I2C, standard /dev/spidev* and /dev/i2c* remain available.
- **Q: 32-bit or 64-bit?** 64-bit aarch64. Pi 5 runs 64-bit Bookworm as default. No need to maintain armhf compatibility.
- **Q: Cross-compilation or native?** Native compilation on the Pi 5 itself. Simplifies dependency resolution and avoids toolchain fragmentation.
- **Q: Do we need a device tree overlay?** Possibly. SPI and I2C can be enabled via /boot/config.txt with `dtparam=spi=on,i2c_arm=on`. PWM needs `dtoverlay=pwm-2chan` or custom overlay. RCIO reset GPIO needs correct pin mapping.
- **Q: How to flash RCIO firmware on Pi 5?** Port Emlid's `blackmagic` SWD bit-bang tool from Pi 2/3/4 GPIO (BCM2708_PERI_BASE + /dev/mem) to Pi 5/RP1 GPIO. The alternative would be an external SWD programmer, but porting blackmagic is the most direct path since it's the same tool Emlid uses and it only needs the GPIO layer rewritten. **UPDATE**: The real problem is deeper — the STM32F103 doesn't respond to SPI at all on Pi 5 (all 0xFF), even though the firmware is present and works on Pi 4. The SWD flash is a secondary concern; first the STM32 must be communicating.

## Closed Decisions

- **Replace pigpio with libgpiod v2**: Standard kernel interface, works on all modern Linux, Pi 5 compatible.
- **64-bit aarch64 only**: No dual-arch burden. Pi 5's native target.
- **Native compilation**: Build on Pi 5 with g++. No cross-compilation needed for this phase.
- **Keep upstream module structure**: Common/ (sensor-agnostic interfaces) + Navio2/ (Navio2-specific implementations). Add a GPIO abstraction layer.
- **SPI/I2C stay as-is**: /dev/spidev* and /dev/i2c* work on Pi 5. The SPIdev.h and I2Cdev.cpp wrappers need only minor adaptation if any.
- **Linux/Pi integration has priority over workarounds**: if a Navio2 peripheral expected by ArduPilot is not working, first fix Linux/Pi/ArduPilot integration rather than bypassing that peripheral.
- **ArduPilot changes are allowed for Navio2-on-Pi-5 compatibility**: when current ArduPilot behavior no longer matches historical Navio2 support, targeted code changes are in scope and should be kept traceable for possible upstreaming.
- **Port blackmagic SWD tool rather than find alternatives**: user preference; the Emlid blackmagic tool is the original flashing mechanism and only requires the GPIO layer to be adapted for RP1

## Future Non-Blocking Improvements (later iteration)

- Add CMake build as alternative to raw Makefile
- Python binding layer using the same libgpiod backend
- CI pipeline with QEMU-user for pre-commit validation
- Docker-based reproducible build environment
- RP1-specific PWM DMA for high-precision servo output

## Deliverables of This Phase

- Modified C++/Navio/Common/gpio.{h,cpp} — GPIO abstraction using libgpiod
- New C++/Navio/Common/gpiod_adapter.{h,cpp} — libgpiod v2 wrapper
- Modified C++/Navio/Makefile — build with aarch64 g++, link -lgpiod instead of -lpigpio
- Modified C++/Makefile — top-level build for aarch64
- Modified get_dependencies.sh — install libgpiod-dev instead of pigpio
- All 8 C++ examples compile and run on Pi 5
- Verification script checking sensor outputs

## Initial Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| RP1 SPI bus has different timing/burst behavior affecting RCIO protocol | High — RCIO co-processor communication breaks | Add configurable SPI speed; verify with logic analyzer if needed |
| libgpiod v2 lacks the fast waveform/PWM API pigpio provided | Medium — PWM may lack precision | Use /sys/class/pwm kernel PWM; accept slower update rate initially |
| MS5611 I2C address collision or bus differences | Low — standard I2C, unlikely to change | Verify with i2cdetect first |
| MPU9250/LSM9DS1 SPI chip select routing differs | Low — same GPIO lines | Verify pin mapping against Navio2 schematic |
| Device tree overlay not loading on Pi 5 kernel | Medium — no SPI/I2C devices | Test with dtparam first, then custom overlay if needed |
| RCIO firmware cannot be flashed because blackmagic SWD tool uses Pi 2/3/4 GPIO mapping | High — ADC, RC input, and full RCIO functionality blocked | Port blackmagic gpio.c to RP1 GPIO register layout or use alternative GPIO access for SWD bit-bang **(DONE — ported, but STM32 still not responding on Pi 5)** |
| STM32F103 does not respond to SPI or SWD on Pi 5 | Critical — all RCIO functionality blocked | Investigate power/reset levels, fix device tree overlay, compare Pi 4 vs Pi 5 SPI signals |
| I2C bus contention between RCIO and MS5611 when both are active | High — barometer becomes unreliable | Investigate after RCIO communication is restored; may need RCIO I2C avoidance or separate addressing |

## Design Phase Exit Criteria

- All design decisions documented as "closed"
- Risks identified with mitigation plans
- Scope clearly bounded
- Build strategy confirmed (native aarch64, libgpiod, Makefile)

Status:
- Design phase: complete

## Current Status

- All Phase 5 work complete. ArduPilot Rover 4.6.3 running on Pi 5 with full Navio2 support: MS5611 barometer, MPU9250 IMU, M8N GPS (3D lock), RCIO PWM/ADC/RCInput, RGB LED.
- All Pi 5 specific changes documented in QUICK_START.md and TECHNICAL_SETUP.md.

## Hardware Platform Decision (2026-05-14)

### Current: Navio2 + Pi 5

- Works, all subsystems validated
- Pi 5 porting cost: 3 kernel-level fixes (drive strength, SPI RX-only, GPIO base), 1 DT overlay, 4 ArduPilot patches
- No official Emlid Pi 5 support — self-maintained going forward
- Sensor note: 1 IMU (MPU9250), 1 baro (MS5611), 0 compass (LSM9DS1 hardware defect on this HAT)

### Alternative considered: Navigator (BlueRobotics)

- https://bluerobotics.com/store/comm-control-power/control/navigator/
- Pi 4 only — same Pi 5 porting risk as Navio2 had, unknown feasibility
- Better hardware design for marine robotics
- Worth evaluating if a hardware upgrade is planned

### Alternative considered: Pixhawk + Pi 5 + Hailo

- Clean separation: Pixhawk handles flight/navigation, Pi 5 handles compute/AI
- MAVLink over UART/USB between them — well-documented, stable
- Best option for a new build; for existing Navio2 setup the porting is done
- Recommended Pixhawk hardware: Pixhawk 6C or Cube Orange+ (avoid clones)

### Custom flight controller

- Viable only with embedded hardware design experience
- Not justified for a single-boat application

### Recommendation

For new builds: **Pixhawk + Pi 5 + Hailo** (clean separation, no porting risk).
For current boat: keep Navio2 + Pi 5 (already working).

## ROS2 Integration Options

### Option 1: ArduPilot DDS (recommended for current setup)

ArduPilot 4.5+ has native micro-ROS/DDS output (`AP_DDS`). It publishes sensor data and accepts control inputs as ROS2 topics directly — no bridge node needed on the ArduPilot side.

```
ArduPilot (navio2 binary) ←→ micro-ROS Agent (Pi 5) ←→ ROS2 topics
```

Enable via:
- `SERIAL5_PROTOCOL = 45` (DDS-XRCE) in ArduPilot params
- Run `micro-ros-agent` on Pi 5

### Option 2: MAVROS2 bridge

MAVROS2 is a ROS2 node that bridges MAVLink ↔ ROS2 topics. More mature, more topics supported, but adds a translation layer.

### Option 3: ROS2 Nav2 + ArduPilot as hardware backend (Jazzy)

The full autonomy stack. ArduPilot handles low-level control (motor mixing, IMU fusion, RC failsafe). ROS2 Nav2 handles high-level navigation (path planning, obstacle avoidance, mission execution). Hailo handles perception (semantic segmentation, object detection).

```
Hailo → segmentation → /perception topics
Nav2 ← /perception + /odometry + /gps → path planning → /cmd_vel
ArduPilot ← /cmd_vel (via DDS or MAVROS2) → motor commands
```

This is the target architecture for full autonomous marine robotics with AI perception.

### Current Pi 5 state

- ROS2 not installed (only pymavlink available)
- Next step: install ROS2 Jazzy (Bookworm packages available), configure AP_DDS

## Publishing the Pi 5 Solution

### RCIO kernel module — compatibility analysis

| File | Change | Pi 4 impact | Safe for both? | Guard |
|---|---|---|---|---|
| `rcio_gpio.c` | `GPIO_CHIP_OFFSET = -1` (dynamic allocation) | None — kernel picks free base | Yes | None needed |
| `rcio-overlay.dts` | `compatible` bcm2709→bcm2712 | Breaks Pi 4 (Pi 4 is bcm2711) | Separate file | `rcio-pi5-overlay.dts` for Pi 5 |
| `rcio_spi.c` | CS delays via `module_param()` | None — defaults to 0 | Yes | Module parameters |
| `rcio_spi.c` | `read_clock_buffer` allocation | Safe — extra memory | Yes | None needed |
| `rcio_spi.c` | `dev_warn` debug messages | Safe — just logging | Yes | None needed |
| `rcio_spi.c` | `remove` returns `void` | Kernel 6.2+ API change | With guard | `LINUX_VERSION_CODE >= 6.2.0` |
| `rcio_core.c` | Error path reorder | Safe — bugfix | Yes | None needed |
| `rcio_core.c` | `EXPORT_SYMBOL_GPL(rcio_state)` | May break out-of-tree modules | Kept | Restored |
| `rcio_pwm.c` | `enable/disable/config` → `apply/get_state` | Upstream already broken on 5.13+ | Yes (bugfix) | None needed |
| `rcio_status.c` | Probe abort on all-failed | Safe — better error handling | Yes | None needed |
| `navio2-led.dts` | New RGB LED overlay | Safe — only loaded if in config.txt | Yes | N/A |

### RCIO PR strategy — two separate PRs

**PR 1 — Bugfixes** (safe for all platforms, can merge quickly):
- `rcio_core.c`: error path cleanup order fix
- `rcio_status.c`: abort probe when STM32 unresponsive
- `rcio_pwm.c`: migrate to `.apply/.get_state` API (fixes existing breakage on kernel 5.13+)
- `rcio_spi.c`: `dev_warn` debug messages + `remove` return type guard

**PR 2 — Pi 5 support** (needs careful review):
- `rcio_gpio.c`: `GPIO_CHIP_OFFSET` platform guard
- `rcio_spi.c`: CS delay macros with `CONFIG_ARCH_BCM2712` guard
- `rcio-pi5-overlay.dts`: separate Pi 5 device tree overlay
- `navio2-led.dts`: RGB LED device tree overlay

### ArduPilot PR — compatibility analysis (pending review)

| File | Change | Pi 4 impact | Safe for both? |
|---|---|---|---|
| `boards.py` | `toolchain = 'native'` | Breaks Pi 4 cross-compilation | Pi 4 can override with `--toolchain=arm-linux-gnueabihf` |
| `AP_Baro_MS5611.cpp` | Skip PROM CRC check | Affects all MS5611 users | Needs navio2 guard |
| `AP_InertialSensor_config.h` | `ALLOW_NO_SENSORS=1` | Changes behavior for all Linux boards | Needs navio2 guard |
| `AP_InertialSensor_NONE.h/cpp` | Enable NONE backend for Linux | Safe — opt-in | Yes (PR #33647) |
| `AP_InertialSensor.cpp` | Warn instead of panic | Changes behavior for all Linux boards | Needs navilo2 guard |
| `HAL_Linux_Class.cpp` | RCOutput_Sysfs pwmchip 0→6 | Breaks Pi 4 (uses pwmchip0) | Needs board config or runtime detection |
| `PWM_Sysfs.cpp` | Retry loop for duty_cycle fd | Safe — retry with timeout | Yes (PR #33647) |
| `board/linux.h` | Default MS5611 I2C bus to 1 | May affect other boards | Needs guard |

### ArduPilot PR strategy — two PRs with per-subsystem commits

Following ArduPilot contribution guidelines (one commit per subsystem, `Subsystem: description` format):

**PR #33647 — Linux bugfixes** (safe for all Linux boards):
- `AP_HAL_Linux: PWM_Sysfs: add retry loop for duty_cycle fd open`
- `AP_InertialSensor: enable NONE backend for HAL_BOARD_LINUX`

**PR #33648 — Navio2 Pi 5 support** (depends on #33647):
- `AP_Baro: skip MS5611 PROM CRC check for Navio2`
- `AP_InertialSensor: allow no sensors and warn instead of panic for Navio2`
- `AP_HAL_Linux: set MS5611 I2C bus and pwmchip index for Navio2`
- `Tools: use native toolchain for navio2 board`

### Steps to publish

1. **RCIO PR #11 (bugfixes)**: [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) — safe for all platforms
2. **RCIO PR #12 (Pi 5 support)**: [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) — dynamic GPIO base, module_param CS delays
3. **ArduPilot PR #33647 (Linux bugfixes)**: [ArduPilot/ardupilot#33647](https://github.com/ArduPilot/ardupilot/pull/33647) — PWM_Sysfs retry, INS NONE backend for Linux
4. **ArduPilot PR #33648 (Navio2 Pi 5)**: [ArduPilot/ardupilot#33648](https://github.com/ArduPilot/ardupilot/pull/33648) — CRC skip, allow no sensors, pwmchip, native toolchain
5. **Public repo**: [axonbf/navio2-rpi5-ardupilot](https://github.com/axonbf/navio2-rpi5-ardupilot) — full setup guide, scripts, overlays, docs
6. **Post on Emlid community forum** (community.emlid.com) — "Navio2 on Pi 5 — working solution"
7. **Post on ArduPilot Discourse** (discuss.ardupilot.org) — targeting Linux board users
8. **Optional**: open issue on Emlid repo requesting official Pi 5 support
