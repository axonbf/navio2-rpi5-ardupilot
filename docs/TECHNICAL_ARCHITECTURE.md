# Technical Architecture

## Document Status

`current`

## Maintenance Standard

This Markdown document with Mermaid diagrams is the source of truth for the technical architecture.

Recommended workflow:
- First edit the Mermaid blocks in this file before implementing changes.

## Architecture-Implementation Traceability Rules

1. Maintain 1:1 correspondence between architectural stages and implementation units (modules, classes or functions).
2. Use code names aligned with architectural blocks to facilitate reading and debugging.
3. Separate responsibilities by modules, files or components with clear boundaries to reduce coupling and facilitate maintenance.
4. Avoid concentrating all logic in the entry point; the main component should act as flow orchestrator.
5. If a name or stage changes in code, update in the same change the pipeline diagram, component diagram and functional blocks sections.
6. Avoid introducing cross-cutting logic that breaks the separation between defined architectural stages.
7. Any new stage must be declared first in architecture and then implemented in code.
8. If a code change affects names, stages, flow, parameters or responsibilities, review and update in the same change the related technical documentation, at minimum this document, `IMPLEMENTATION_PLAN.md` and `TECHNICAL_SETUP.md` when applicable.
9. Keep logs and errors aligned with architectural stages, using consistent prefixes or categories that facilitate traceability, debugging and flow validation.

## Team Mermaid Standard

1. Source of truth: maintain embedded diagrams in this `.md` and avoid copies in other files.
2. Visual unification: reuse the same `%%{init: ...}%%` block in all diagrams to support both light and dark modes.
3. Consistent names: use the same stage labels in diagram and text throughout the document.
4. Synchronized changes: any flow change must update at the same time the pipeline diagram, component diagram and functional blocks sections.
5. Avoid redundancies: do not repeat the same technical detail in several sections; maintain a main section and reference the others.
6. Complexity scale: if a diagram exceeds ~40 nodes, divide it into two smaller diagrams by functional domain.
7. Minimum review before closing: validate render in Markdown preview and check legibility on normal screen (without zoom).

## Objective

Describe the technical architecture of the Navio2 + Raspberry Pi 5 project: hardware abstraction layers, device communication pipeline, boot/setup sequence, and ArduPilot integration.

## System Architecture

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 40, "rankSpacing": 55, "htmlLabels": true}}}%%
flowchart TB
    subgraph PI5["Raspberry Pi 5"]
        subgraph BOOT["Boot Sequence"]
            B1["rp1-spi1-drive.service<br/>Sets GPIO16/19/20/21 to 12 mA"]
            B2["rcio-startup.sh<br/>Loads rcio_core.ko + rcio_spi.ko<br/>Disables RT throttling"]
            B3["ardurover.service<br/>Starts ArduPilot Rover"]
            B1 --> B2 --> B3
        end

        subgraph KERNEL["Linux Kernel 6.6"]
            K1["rcio_core.ko<br/>ADC, PWM, RCInput, GPIO, Status"]
            K2["rcio_spi.ko<br/>SPI bridge to STM32"]
            K3["spi_dw / dw_spi_mmio<br/>RP1 SPI1 controller"]
            K4["spidev0<br/>SPI0 for MPU9250 + M8N GPS"]
            K5["i2c-dev<br/>I2C bus 1 for MS5611"]
            K6["gpio-leds<br/>rgb_led0/1/2 via DT overlay"]
            K2 --> K3
            K1 --> K2
        end

        subgraph USER["Userspace"]
            U1["ArduPilot Rover 4.6.3<br/>navio2 board subtype"]
            U2["QGroundControl<br/>via UDP MAVLink"]
            U1 <-->|MAVLink| U2
        end
    end

    subgraph HAT["Navio2 HAT Hardware"]
        H1["STM32F103 RCIO<br/>PWM x14, ADC x6, RCInput x16"]
        H2["MPU9250 IMU<br/>SPI0 CS1"]
        H3["U-blox M8N GPS<br/>SPI0 CS0"]
        H4["MS5611 Barometer<br/>I2C bus 1 addr 0x77"]
        H5["LSM9DS1 IMU<br/>DEFECTIVE (WHO_AM_I=0xFF)"]
        H6["RGB LED<br/>GPIO4=R, GPIO6=B, GPIO27=G"]
    end

    B1 -.->|12 mA drive| H1
    K3 <-->|SPI1| H1
    K4 <-->|SPI0| H2
    K4 <-->|SPI0| H3
    K5 <-->|I2C| H4
    K6 -->|sysfs| H6
    U1 -->|/sys/class/pwm| K1
    U1 -->|/dev/spidev0| K4
    U1 -->|/dev/i2c-1| K5
    U1 -->|/sys/class/leds| K6
```

## Boot Sequence (Pi 5 Specific)

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 40, "rankSpacing": 55, "htmlLabels": true}}}%%
flowchart LR
    subgraph DT["Device Tree"]
        D1["spi1-1cs overlay<br/>cs0_pin=16, spidev=disabled"]
        D2["rcio-pi5 overlay<br/>bcm2712, spi@54000"]
        D3["navio2-led overlay<br/>GPIO4/6/27 as gpio-leds"]
    end

    subgraph SYSINIT["Sysinit Phase"]
        S1["rp1-spi1-drive.service<br/>Writes RP1 pad registers<br/>GPIO16/19/20/21 = 12 mA"]
    end

    subgraph STARTUP["rcio-startup.sh"]
        R1["Disable RT throttling<br/>sched_rt_runtime_us = -1"]
        R2["insmod rcio_core.ko"]
        R3["insmod rcio_spi.ko"]
        R4["Wait for alive=1"]
        R1 --> R2 --> R3 --> R4
    end

    subgraph ARDU["ArduPilot"]
        A1["ardurover binary<br/>navio2 board subtype"]
        A2["Detect MS5611 I2C"]
        A3["Detect MPU9250 SPI"]
        A4["Detect M8N GPS SPI"]
        A5["Export 14 PWM channels"]
        A6["RGB LED status"]
        A7["MAVLink UDP telemetry"]
        A1 --> A2 --> A3 --> A4 --> A5 --> A6 --> A7
    end

    DT --> SYSINIT --> STARTUP --> ARDU
```

## Technical Pipeline

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 40, "rankSpacing": 55, "htmlLabels": true}}}%%
flowchart LR
    subgraph APP["ArduPilot Rover 4.6.3"]
        A1["Barometer"]
        A2["IMU / EKF3"]
        A3["GPS Navigation"]
        A4["PWM Servo Output"]
        A5["RC Input"]
        A6["ADC"]
        A7["RGB LED Status"]
    end

    subgraph KERNEL["Linux Kernel Interfaces"]
        K1["/dev/i2c-1<br/>I2C bus 1"]
        K2["/dev/spidev0.0<br/>SPI0 CS0"]
        K3["/dev/spidev0.1<br/>SPI0 CS1"]
        K4["/sys/class/pwm/pwmchip6<br/>RCIO PWM 14ch"]
        K5["/sys/kernel/rcio/<br/>ADC + RCInput sysfs"]
        K6["/sys/class/leds/<br/>rgb_led0/1/2"]
    end

    subgraph HW["Navio2 HAT"]
        H1["MS5611<br/>Baro (I2C 0x77)"]
        H2["M8N GPS<br/>SPI0 CS0"]
        H3["MPU9250<br/>IMU (SPI0 CS1)"]
        H4["STM32F103 RCIO<br/>PWM + ADC + RCInput (SPI1)"]
        H5["RGB LED<br/>GPIO4/6/27"]
    end

    A1 --> K1 --> H1
    A3 --> K2 --> H2
    A2 --> K3 --> H3
    A4 --> K4 --> H4
    A5 --> K5 --> H4
    A6 --> K5 --> H4
    A7 --> K6 --> H5
```

## Technical Component Diagram

```mermaid
%%{init: {"flowchart": {"nodeSpacing": 40, "rankSpacing": 55, "htmlLabels": true}}}%%
flowchart TB
    subgraph RCIO["rcio-dkms kernel module"]
        RC1["rcio_spi.ko<br/>SPI bridge to STM32"]
        RC2["rcio_core.ko"]
        RC3["rcio_pwm.c<br/>.apply/.get_state API"]
        RC4["rcio_adc.c"]
        RC5["rcio_rcin.c"]
        RC6["rcio_gpio.c<br/>GPIO_CHIP_OFFSET = -1 (dynamic)"]
        RC7["rcio_status.c<br/>Probe + CRC check"]
        RC8["rcio_safety.c"]
        RC2 --- RC3
        RC2 --- RC4
        RC2 --- RC5
        RC2 --- RC6
        RC2 --- RC7
        RC2 --- RC8
    end

    subgraph DRV["Navio2 C++ Drivers (libnavio.a)"]
        D1["Common/<br/>SPIdev, I2Cdev, MPU9250<br/>MS5611, Ublox, gpio.cpp"]
        D2["Navio2/<br/>PWM, ADC_Navio2<br/>RCInput, RGBled"]
    end

    subgraph ARDU["ArduPilot (navio2 board)"]
        AR1["AP_Baro_MS5611<br/>CRC skip patch"]
        AR2["AP_InertialSensor<br/>ALLOW_NO_SENSORS + NONE backend"]
        AR3["RCOutput_Sysfs<br/>pwmchip6"]
        AR4["SPIUARTDriver<br/>serial3 = ublox GPS"]
        AR5["Led_Sysfs<br/>rgb_led0/1/2"]
    end

    subgraph DT["Device Tree Overlays"]
        T1["rcio-pi5-overlay.dts<br/>bcm2712, SPI1"]
        T2["navio2-led.dts<br/>GPIO4/6/27 as gpio-leds"]
        T3["spi1-1cs<br/>cs0_pin=16"]
    end

    subgraph SVC["Systemd Services"]
        SV1["rp1-spi1-drive.service<br/>12 mA drive strength"]
        SV2["ardurover.service<br/>Auto-start ArduPilot"]
    end
```

## Functional Blocks

### 1. Boot and Device Tree

- **Device tree overlays** loaded from `/boot/firmware/config.txt`:
  - `spi1-1cs,cs0_pin=16,cs0_spidev=disabled` — enables RP1 SPI1 with CS on GPIO16
  - `rcio-pi5` — binds RCIO driver to SPI1 device (bcm2712 compatible)
  - `navio2-led` — creates `/sys/class/leds/rgb_led{0,1,2}` from GPIO4/6/27
- **Blacklist**: `/etc/modprobe.d/blacklist-rcio.conf` prevents RCIO auto-load at boot

### 2. RP1 Drive Strength Fix

- **Component**: `/usr/local/bin/rp1-spi1-drive.py` + `/etc/systemd/system/rp1-spi1-drive.service`
- **Problem**: RP1 GPIO defaults to 4 mA; BCM2711 (Pi 4) defaults to 16 mA. Navio2 SPI traces need 12 mA for reliable STM32 communication.
- **Fix**: systemd service runs at `sysinit.target`, writes 12 mA to RP1 pad registers for GPIO16/19/20/21
- **Without this**: RCIO `alive=0` — STM32 does not respond

### 3. RCIO Kernel Module

- **Components**: `rcio_core.ko` + `rcio_spi.ko` (built from emlid/rcio-dkms with Pi 5 patches)
- **Pi 5 specific changes** (all platform-guarded):
  - `rcio_gpio.c`: `GPIO_CHIP_OFFSET = -1` (dynamic allocation — kernel picks free base, no platform guards needed)
  - `rcio_spi.c`: CS delays via `module_param()` (defaults to 0; Pi 5 passes values via `insmod`)
  - `rcio_spi.c`: `remove` return type `void` on kernel 6.2+ (`LINUX_VERSION_CODE` guard)
  - `rcio_pwm.c`: `.apply/.get_state` API (old `.enable/.disable/.config` removed in kernel 5.13+)
  - `rcio-pi5-overlay.dts`: separate overlay for Pi 5 (bcm2712)
- **Loaded by**: `/home/pi/rcio-startup.sh` (also disables RT throttling)
- **Verified state**: `alive=1`, `board_name=navio2`, `crc=0xb9064332`

### 4. RCIO Communication (SPI1)

- **Bus**: RP1 SPI1 at `/axi/pcie@120000/rp1/spi@54000` via `dw_spi_mmio` / `spi_dw`
- **Pins**: GPIO16=CS (pin 36), GPIO19=MISO (pin 35), GPIO20=MOSI (pin 38), GPIO21=SCLK (pin 40)
- **Protocol**: RCIO packet structure with CRC8, write-then-read via `spi_write_then_read()`
- **Two root causes that were fixed**:
  1. RP1 GPIO 4 mA default → 12 mA fix
  2. Ported `rcio_spi.c` sent 0xFF on MOSI during read → restored RX-only `wait_complete()`

### 5. Sensor Interfaces

| Sensor | Bus | Device | Status |
|---|---|---|---|
| MS5611 barometer | I2C bus 1 | `/dev/i2c-1` addr 0x77 | Working (CRC skip patch) |
| MPU9250 IMU | SPI0 CS1 | `/dev/spidev0.1` | Working, calibrated via QGC |
| U-blox M8N GPS | SPI0 CS0 | `/dev/spidev0.0` | Working, 3D lock 11+ sats |
| LSM9DS1 IMU | SPI0 CS2/CS3 | — | **Hardware defect** (WHO_AM_I=0xFF), auto-skipped |
| RCIO ADC | SPI1 | `/sys/kernel/rcio/adc/ch0-5` | Working |
| RCIO RCInput | SPI1 | `/sys/kernel/rcio/rcin/ch0-15` | Working |
| RCIO PWM | SPI1 | `/sys/class/pwm/pwmchip6` 14ch | Working |
| RGB LED | GPIO | `/sys/class/leds/rgb_led{0,1,2}` | Working |

### 6. ArduPilot Rover Integration

- **Binary**: `~/ardupilot/build/navio2/bin/ardurover` (navio2 board subtype, aarch64 native)
- **Source patches** (4 files, applied to ArduPilot 4.6.3):
  - `AP_Baro_MS5611.cpp` — skip PROM CRC check (Navio2 MS5611 returns CRC=0)
  - `AP_InertialSensor_config.h` — `AP_INERTIALSENSOR_ALLOW_NO_SENSORS 1`
  - `AP_InertialSensor_NONE.h/cpp` — enable NONE backend for Linux
  - `AP_InertialSensor.cpp` — warn instead of panic on missing IMU
- **Systemd service**: `/etc/systemd/system/ardurover.service` with `ExecStartPre=/home/pi/rcio-startup.sh`
- **Config**: `/etc/default/ardurover` (serial ports, telemetry IP, `--serialN` syntax)
- **Params**: `~/ardurover_work/boat_navio2.parm`
- **RT throttling**: `sched_rt_runtime_us = -1` (standard for ArduPilot on Linux)

### 7. RGB LED

- **Device tree overlay**: `navio2-led.dtbo` creates `/sys/class/leds/rgb_led{0,1,2}`
  - `rgb_led0` = Red (GPIO4)
  - `rgb_led1` = Blue (GPIO6)
  - `rgb_led2` = Green (GPIO27)
- **ArduPilot driver**: `Led_Sysfs` class writes brightness to sysfs
- **Status colors**: solid green = ready, flashing = GPS lock, etc.

### 8. ROS2 Integration (planned, not yet implemented)

- **Target**: ROS2 Jazzy + Nav2 + ArduPilot DDS + Hailo-8L inference
- **Architecture**: ArduPilot (hardware) ← DDS → Nav2 (navigation) ← ROS2 topics → Hailo (perception)
- **Current state**: ROS2 not installed; only `pymavlink` available
- **See**: `TECHNICAL_DESIGN.md` → ROS2 Integration Options

## Pi 4 vs Pi 5 Hardware Differences

| Aspect | Pi 4 (BCM2711) | Pi 5 (BCM2712 + RP1) |
|---|---|---|
| GPIO drive strength | 16 mA default | 4 mA default → 12 mA fix needed |
| SPI1 controller | BCM SoC `/soc/spi@7e215080` | RP1 `/axi/pcie@120000/rp1/spi@54000` |
| SPI1 driver | `bcm2835-aux-spi` | `dw_spi_mmio` / `spi_dw` |
| GPIO base for RCIO | 500 (no overlap) | Dynamic (`-1`) — kernel assigns 625+ |
| PWM ops API | `.enable/.disable/.config` | `.apply/.get_state` (kernel 5.13+) |
| Device tree overlay | `rcio-overlay.dts` (bcm2709) | `rcio-pi5-overlay.dts` (bcm2712) |
| Kernel | 5.x | 6.6 |

## Upstream PRs

| PR | Repo | Status | Description |
|---|---|---|---|
| [emlid/rcio-dkms#11](https://github.com/emlid/rcio-dkms/pull/11) | emlid/rcio-dkms | Open | Bugfixes: error path, PWM API, probe abort, debug messages |
| [emlid/rcio-dkms#12](https://github.com/emlid/rcio-dkms/pull/12) | emlid/rcio-dkms | Open | Pi 5 platform support (dynamic GPIO base, module_param CS delays) |
| [ArduPilot/ardupilot#33647](https://github.com/ArduPilot/ardupilot/pull/33647) | ArduPilot/ardupilot | Open | Linux bugfixes: PWM_Sysfs retry, INS NONE backend for Linux |
| [ArduPilot/ardupilot#33648](https://github.com/ArduPilot/ardupilot/pull/33648) | ArduPilot/ardupilot | Open | Navio2 Pi 5: CRC skip, allow no sensors, pwmchip, native toolchain |
| [axonbf/navio2-rpi5-ardupilot](https://github.com/axonbf/navio2-rpi5-ardupilot) | Public repo | Live | Full setup guide, scripts, overlays, documentation |