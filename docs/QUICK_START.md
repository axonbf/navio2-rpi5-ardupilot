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
- LSM9DS1 is defective on this HAT — ArduPilot auto-skips it

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

# Navio2 RGB LED (GPIO4=Red, GPIO6=Blue, GPIO27=Green)
dtoverlay=navio2-led
```

The `rcio-pi5.dtbo` overlay must be in `/boot/firmware/overlays/`.
The `navio2-led.dtbo` overlay must also be in `/boot/firmware/overlays/`.
Both are in this repo — compile and install:

```bash
dtc -@ -I dts -O dtb -o /boot/firmware/overlays/rcio-pi5.dtbo rcio_source/rcio-overlay.dts
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

**Pi 5 specific change**: `rcio_gpio.c` uses `GPIO_CHIP_OFFSET 420` on Pi 5 (via `#ifdef CONFIG_ARCH_BCM2712`), 500 on Pi 4.
The source in this repo is platform-guarded — it compiles correctly for both Pi 4 and Pi 5.
The `rcio-pi5-overlay.dts` is a separate overlay for Pi 5 (bcm2712); the original `rcio-overlay.dts` (bcm2709) is kept for Pi 4.

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
    insmod /home/pi/rcio_build/rcio_spi.ko
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

```bash
cd ~/ardupilot
./waf configure --board navio2 --define=HAL_BARO_MS5611_I2C_BUS=1
./waf build -j4
```

**Required source patches** (already applied in the project fork):
1. `AP_Baro_MS5611.cpp` — skip PROM CRC check (Navio2 MS5611 returns CRC=0)
2. `AP_InertialSensor_config.h` — `AP_INERTIALSENSOR_ALLOW_NO_SENSORS 1`
3. `AP_InertialSensor_NONE.h/cpp` — enable dummy INS backend for Linux
4. `AP_InertialSensor.cpp` — warn instead of panic when no IMU found

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
| RCIO GPIO base 420 | `rcio_source/src/rcio_gpio.c` (`#ifdef CONFIG_ARCH_BCM2712`) | Pi 5 RP1 GPIO starts at base 512; original 500 overlaps |
| RCIO SPI CS delays | `rcio_source/src/rcio_spi.c` (`#ifdef CONFIG_ARCH_BCM2712`) | Pi 5 RP1 needs 50µs CS setup/hold; Pi 4 defaults to 0 |
| RCIO PWM API migration | `rcio_source/src/rcio_pwm.c` | `.apply/.get_state` replaces `.enable/.disable/.config` (removed in kernel 5.13+) |
| RCIO `remove` return type | `rcio_source/src/rcio_spi.c` (`LINUX_VERSION_CODE >= 6.2.0`) | Kernel 6.2+ changed `spi_driver.remove` to return void |
| Separate Pi 5 overlay | `rcio_source/rcio-pi5-overlay.dts` | Pi 5 uses bcm2712; original `rcio-overlay.dts` (bcm2709) kept for Pi 4 |
| RGB LED device tree | `rcio_source/navio2-led.dts` | Creates `/sys/class/leds/rgb_led0/1/2` for ArduPilot status LED |
| RCIO blacklisted at boot | `/etc/modprobe.d/blacklist-rcio.conf` | Prevents I2C contention; loaded by rcio-startup.sh |
| RT throttling disabled | `rcio-startup.sh` sets `sched_rt_runtime_us=-1` | Standard for ArduPilot on Linux |
| ArduPilot CRC skip | `AP_Baro_MS5611.cpp` | Navio2 MS5611 returns CRC=0 in PROM word 7 |
| ArduPilot INS fallback | `AP_InertialSensor_*` patches | Allow boot without LSM9DS1; warn instead of panic |

---

## Files in this repository

| Path | Purpose |
|---|---|
| `src/` | Navio2 C++ driver library and examples (ported to libgpiod) |
| `rcio_source/` | RCIO kernel module source (Pi 5 port, GPIO base 420, RX-only fix) |
| `rcio_source/navio2-led.dts` | Device tree overlay for RGB LED sysfs entries |
| `src/rp1-spi1-drive.py` | RP1 12 mA drive-strength script |
| `src/navio2_pi5_setup.sh` | Full automated setup script |
| `upstream/` | Reference clone of emlid/Navio2 |
| `docs/` | Project documentation |

---

## Known hardware defect

**LSM9DS1**: WHO_AM_I returns `0xFF` (hardware failure on this HAT).
ArduPilot detects this at startup and silently skips it. MPU9250 IMU works normally.

---

## I2C + RCIO note

MS5611 and RCIO coexist on I2C bus 1 without issues when both are loaded.
The earlier blacklist was a precaution during bring-up and is no longer needed for contention reasons,
but is kept to ensure clean boot sequencing (RCIO loads after drive-strength service).