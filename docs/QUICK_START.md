# Navio2 + Raspberry Pi 5 — Quick Start

## Document Status

`current`

## What this gives you

A Raspberry Pi 5 running ArduPilot Rover with full Navio2 HAT support:
- MS5611 barometer (I2C)
- U-blox M8N GPS (SPI, 3D lock confirmed)
- MPU9250 IMU (SPI, calibrated via QGC)
- RCIO coprocessor (PWM ×14, ADC ×6, RC input ×16)
- RGB LED status indicator
- Compass: LSM9DS1 magnetometer + AK8963 (both via the `navio2-spi0-cs2` overlay)
- LSM9DS1 accel/gyro is **not** used as an IMU — ArduPilot runs on the MPU9250 (2nd-IMU deferred; not a hardware defect — see Sensor notes)

**Target**: boat autopilot, ArduRover 4.6.3 `navio2` board subtype, Pi 5 Bookworm 64-bit.

---

## Hardware

- Raspberry Pi 5 (any RAM)
- Navio2 autopilot HAT
- Raspberry Pi OS Bookworm 64-bit (64-bit required)
- Tested kernel: `6.6.51+rpt-rpi-2712`

---

## Step 1 — Flash and configure OS

Standard Raspberry Pi Imager. Enable SSH. Then:

```bash
# Enable SPI and I2C in /boot/firmware/config.txt
dtparam=spi=on
dtparam=i2c_arm=on
```

---

## Step 2 — Add overlays to `/boot/firmware/config.txt`

```ini
# RCIO SPI1 (order matters — drive-strength service runs before RCIO)
dtoverlay=spi1-1cs,cs0_pin=16,cs0_spidev=disabled
dtoverlay=rcio-pi5

# Navio2 SPI0 3rd chip-select on GPIO22 → creates /dev/spidev0.2 for the
# LSM9DS1 magnetometer. Pi 5's SPI0 exposes only 2 CS by default; without
# this, enabling the compass panics on a missing /dev/spidev0.2.
dtoverlay=navio2-spi0-cs2

# Navio2 RGB LED (GPIO4=Red, GPIO6=Blue, GPIO27=Green)
dtoverlay=navio2-led
```

The `.dtbo` overlays must be in `/boot/firmware/overlays/`. They are all in this repo — compile and install:

```bash
dtc -@ -I dts -O dtb -o /boot/firmware/overlays/rcio-pi5.dtbo rcio_source/rcio-pi5-overlay.dts
dtc -@ -I dts -O dtb -o /boot/firmware/overlays/navio2-spi0-cs2.dtbo rcio_source/navio2-spi0-cs2.dts
dtc -@ -I dts -O dtb -o /boot/firmware/overlays/navio2-led.dtbo rcio_source/navio2-led.dts
```

---

## Step 3 — Install RP1 drive-strength service

**Critical**: RP1 GPIO defaults to 4 mA. Navio2 SPI to STM32 requires 12 mA.

```bash
sudo cp src/rp1-spi1-drive.py /usr/local/bin/rp1-spi1-drive.py
sudo chmod +x /usr/local/bin/rp1-spi1-drive.py

sudo tee /etc/systemd/system/rp1-spi1-drive.service > /dev/null << 'EOF'
[Unit]
Description=Set RP1 SPI1 GPIO drive strength to 12mA for Navio2 RCIO
DefaultDependencies=no
Before=basic.target
After=local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/rp1-spi1-drive.py
RemainAfterExit=yes

[Install]
WantedBy=sysinit.target
EOF

sudo systemctl enable rp1-spi1-drive.service
```

---

## Step 4 — Build and install RCIO kernel modules

```bash
cd rcio_source
sudo apt-get install -y build-essential linux-headers-$(uname -r)
make clean && make
# Produces: rcio_core.ko, rcio_spi.ko
sudo mkdir -p /home/pi/rcio_build
sudo cp *.ko /home/pi/rcio_build/
```

**Pi 5 specific**: `rcio_gpio.c` uses `GPIO_CHIP_OFFSET = -1` (dynamic allocation — kernel picks a free base automatically). No platform guards needed.
The `rcio-pi5-overlay.dts` is a separate overlay for Pi 5 (bcm2712); the original `rcio-overlay.dts` (bcm2709) is kept for Pi 4.
CS delays are passed as module parameters at load time (see Step 6).

---

## Step 5 — Blacklist RCIO for safe boot

```bash
sudo tee /etc/modprobe.d/blacklist-rcio.conf > /dev/null << 'EOF'
blacklist rcio_core
blacklist rcio_spi
install rcio_core /bin/true
install rcio_spi /bin/true
EOF
```

---

## Step 6 — Create RCIO startup script

```bash
sudo tee /home/pi/rcio-startup.sh > /dev/null << 'EOF'
#!/bin/bash
set -e
echo -1 > /proc/sys/kernel/sched_rt_runtime_us
if ! lsmod | grep -q rcio_spi; then
    insmod /home/pi/rcio_build/rcio_core.ko
    insmod /home/pi/rcio_build/rcio_spi.ko cs_setup_us=50 cs_hold_us=50 cs_inactive_us=500
fi
for i in $(seq 1 10); do
    if [ -f /sys/kernel/rcio/status/alive ]; then
        ALIVE=$(cat /sys/kernel/rcio/status/alive)
        if [ "$ALIVE" = "1" ]; then
            echo "RCIO ready: alive=1"
            exit 0
        fi
    fi
    sleep 0.5
done
echo "WARNING: RCIO did not report alive=1, continuing anyway"
exit 0
EOF
sudo chmod +x /home/pi/rcio-startup.sh
```

The `cs_setup_us=50 cs_hold_us=50 cs_inactive_us=500` parameters are Pi 5 specific — they configure the CS delays needed for RP1 SPI timing. On Pi 4, these parameters are not needed (defaults to 0).

---

## Step 7 — Reboot and verify

```bash
sudo reboot

# After reboot:
systemctl status rp1-spi1-drive.service   # must be: active (exited)
/home/pi/rcio-startup.sh                  # loads RCIO + disables RT throttling
cat /sys/kernel/rcio/status/alive         # must be: 1
cat /sys/kernel/rcio/status/board_name     # must be: navio2
cat /sys/kernel/rcio/status/crc            # must be: 0xb9064332
ls /sys/class/leds/                         # must include: rgb_led0  rgb_led1  rgb_led2
```

---

## Step 8 — Build ArduPilot Rover

### Get the source (pinned)

Sensors need **no** patch — upstream `navio2` hwdef already declares MPU9250 + LSM9DS1 mag + AK8963. Only two small ArduPilot changes are required, and both live as open PRs on **this project's own fork** (`axonbf/ardupilot`). Because they are on your account, the branches stay available even if the PRs are never approved. Pin them by exact commit so a fresh build reproduces this status even if a branch is later force-pushed:

```bash
git clone https://github.com/ArduPilot/ardupilot.git ~/ardupilot
cd ~/ardupilot
git remote add axonbf https://github.com/axonbf/ardupilot.git
git fetch axonbf pr-navio2-pwmchip pr-pwm-sysfs-retry
git cherry-pick 84182acccb82dabbb771afcb30d400bc742282d9   # C: RCIO pwmchip runtime detect (Pi4→pwmchip0, Pi5→pwmchip6) — PR #33655
git cherry-pick 15967de4b9fdde3d7b99bfa533e6c22fe701c66e   # G: PWM_Sysfs duty_cycle retry on slow sysfs export      — PR #33656
git submodule update --init --recursive
```

Both are single, self-contained commits. If either PR is merged upstream later, skip its cherry-pick (it becomes a no-op).

### Configure + build

On a 64-bit Pi you must select the native toolchain — the `navio2` board otherwise defaults to the 32-bit `arm-linux-gnueabihf` cross-compiler:

```bash
cd ~/ardupilot
./waf configure --board navio2 --toolchain=native
./waf build -j4
```

On current ArduPilot master the baro I²C bus is set by the board hwdef. On older releases (e.g. 4.6.x) also add `--define=HAL_BARO_MS5611_I2C_BUS=1`.

The earlier MS5611 CRC-skip and INS "allow no sensors" patches are **no longer used** — validated unnecessary (the MS5611 PROM CRC is valid on this hardware, and ArduPilot runs on the working MPU9250).

---

## Step 9 — Create systemd service for ArduRover

```bash
sudo ln -sf /home/pi/ardupilot/build/navio2/bin/ardurover /usr/bin/ardurover

sudo tee /etc/default/ardurover > /dev/null << 'EOF'
# GCS telemetry via UDP
TELEM1="--serial1 udp:192.168.178.20:14550"
# H12 RC receiver on UART
TELEM2="--serial2 /dev/ttyAMA0"
ARDUPILOT_OPTS="$TELEM1 $TELEM2"
EOF

sudo tee /etc/systemd/system/ardurover.service > /dev/null << 'EOF'
[Unit]
Description=ArduPilot Rover on Navio2 + Pi 5
After=rp1-spi1-drive.service
Wants=rp1-spi1-drive.service

[Service]
EnvironmentFile=/etc/default/ardurover
ExecStartPre=/home/pi/rcio-startup.sh
ExecStart=/bin/sh -c "/usr/bin/ardurover ${ARDUPILOT_OPTS} --defaults /home/pi/ardurover_work/boat_navio2.parm"
Restart=on-failure
RestartSec=5
LimitRTPRIO=98
LimitRTTIME=infinity

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable ardurover
```

---

## Step 10 — Run

```bash
# Manual:
sudo ardurover --serial1 udp:<GCS_IP>:14550 --defaults ~/ardurover_work/boat_navio2.parm

# Or via systemd:
sudo systemctl start ardurover
sudo systemctl stop ardurover
```

---

## Pi 5 specific changes (summary for fork/contribution)

| Change | File | Why |
|---|---|---|
| GPIO drive strength 12 mA | `src/rp1-spi1-drive.py` + systemd service | RP1 defaults to 4 mA; Navio2 needs 12 mA for STM32 SPI |
| RCIO GPIO base | `rcio_source/src/rcio_gpio.c` (`GPIO_CHIP_OFFSET = -1`) | Dynamic allocation — kernel picks free base on any platform |
| RCIO SPI CS delays | `rcio_source/src/rcio_spi.c` (`module_param`) | Pi 5 RP1 needs 50µs CS setup/hold; passed via `insmod` parameters |
| RCIO PWM API migration | `rcio_source/src/rcio_pwm.c` | `.apply/.get_state` replaces `.enable/.disable/.config` (removed in kernel 5.13+) |
| RCIO `remove` return type | `rcio_source/src/rcio_spi.c` (`LINUX_VERSION_CODE >= 6.2.0`) | Kernel 6.2+ changed `spi_driver.remove` to return void |
| Separate Pi 5 overlay | `rcio_source/rcio-pi5-overlay.dts` | Pi 5 uses bcm2712; original `rcio-overlay.dts` (bcm2709) kept for Pi 4 |
| Navio2 SPI0 3rd CS | `rcio_source/navio2-spi0-cs2.dts` (GPIO22) | Creates `/dev/spidev0.2` for the LSM9DS1 magnetometer; Pi 5 SPI0 has only 2 CS by default. Without it, enabling the compass panics. |
| RGB LED device tree | `rcio_source/navio2-led.dts` | Creates `/sys/class/leds/rgb_led0/1/2` for ArduPilot status LED |
| RCIO blacklisted at boot | `/etc/modprobe.d/blacklist-rcio.conf` | Prevents I2C contention; loaded by rcio-startup.sh |
| RT throttling disabled | `rcio-startup.sh` sets `sched_rt_runtime_us=-1` | Standard for ArduPilot on Linux |
| ArduPilot RCIO pwmchip | `AP_HAL_Linux/HAL_Linux_Class.cpp` | Runtime pwmchip detection (Pi 4 = 0, Pi 5 = 6) — PR #33655 |
| ArduPilot PWM_Sysfs retry | `AP_HAL_Linux/PWM_Sysfs.cpp` | Retry duty_cycle open for slow sysfs export — PR #33656 |

### Pinned commits (reproducibility)

Every external dependency is pinned so a clean SD card reproduces this exact status. The RCIO kernel module **and all overlays are vendored in `rcio_source/` in this repo** — no external fetch needed for them. Only ArduPilot is fetched, from this project's fork:

| Change | Fork (your account) | Branch | Commit | PR | How it's obtained |
|---|---|---|---|---|---|
| RCIO pwmchip runtime detect | `axonbf/ardupilot` | `pr-navio2-pwmchip` | `84182ac` | #33655 | cherry-pick (Step 8) |
| PWM_Sysfs duty_cycle retry | `axonbf/ardupilot` | `pr-pwm-sysfs-retry` | `15967de` | #33656 | cherry-pick (Step 8) |
| RCIO bugfixes | `axonbf/rcio-dkms` | `bugfixes` | `e2d2c36` | #11 | vendored in `rcio_source/` |
| RCIO Pi 5 support | `axonbf/rcio-dkms` | `pi5-support-v2` | `3af18f3` | #12 | vendored in `rcio_source/` |

The forks are on **your** GitHub account, so these branches remain available whether or not the upstream PRs are ever approved. Pinning the commit SHA additionally protects against a later force-push of your own branch. The two `rcio-dkms` commits are recorded for provenance only — reproduction builds the module directly from `rcio_source/`.

> **Note:** the `rcio_source/src/` module is byte-identical to the fork branch, but the compass overlay `navio2-spi0-cs2.dts` lives **only** in this repo — it is a Navio2 SPI0 chip-select overlay, not part of the RCIO module, so it is intentionally absent from `axonbf/rcio-dkms`. Always build the RCIO side from `rcio_source/` (Step 4), not from the fork, or the 2nd compass will be missing.

---

## Files in this repository

| Path | Purpose |
|---|---|
| `src/` | Navio2 C++ driver library and examples (ported to libgpiod) |
| `rcio_source/` | RCIO kernel module source (Pi 5 port, dynamic GPIO base, module_param CS delays) |
| `rcio_source/navio2-led.dts` | Device tree overlay for RGB LED sysfs entries |
| `src/rp1-spi1-drive.py` | RP1 12 mA drive-strength script |
| `src/navio2_pi5_setup.sh` | Full automated setup script |
| `upstream/` | Reference clone of emlid/Navio2 |
| `docs/` | Project documentation |

---

## Sensor notes

**LSM9DS1 magnetometer**: works as the 2nd compass. It was unreachable on Pi 5 until the `navio2-spi0-cs2` overlay created `/dev/spidev0.2`; now the LSM9DS1 mag + AK8963 are both detected as compasses.

**LSM9DS1 accel/gyro (2nd IMU) — not a hardware defect (verified 2026-07-05).** With a dedicated chip-select (`spidev0.3`) the accel reads valid ~1 g data over raw SPI at every speed. But ArduPilot's LSM9DS1 **IMU driver** never reads it at runtime on Pi 5 / RP1 — its periodic callback never fires — so the instance streams zeros and, with `INS_ENABLE_MASK=3`, stalls gyro-cal at boot. The 2nd IMU is therefore **deferred to future work** (upstream driver issue, not the chip). This build uses the MPU9250 as the sole IMU (`INS_ENABLE_MASK=1`). Without a 4th chip-select, `WHO_AM_I` reads `0xFF` and ArduPilot skips it automatically. See `docs/TODO.md`.

---

## I2C + RCIO note

MS5611 and RCIO coexist on I2C bus 1 without issues when both are loaded.
The earlier blacklist was a precaution during bring-up and is no longer needed for contention reasons,
but is kept to ensure clean boot sequencing (RCIO loads after drive-strength service).