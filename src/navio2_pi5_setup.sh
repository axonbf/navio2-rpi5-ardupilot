#!/bin/bash
# =============================================================================
# Navio2 + Pi 5 Setup Script
# =============================================================================
# Configures a fresh Raspberry Pi 5 (Bookworm 64-bit) for Navio2 HAT + ArduPilot.
# Run as: sudo bash navio2_pi5_setup.sh
#
# What this does:
#   1. Installs the RP1 GPIO drive-strength service (12mA for SPI1 pins)
#   2. Adds required dtoverlays to /boot/firmware/config.txt
#   3. Blacklists RCIO modules for safe boot (manual load only)
#   4. Builds RCIO kernel modules from source
#   5. Runs a quick post-setup verification
#
# Assumptions:
#   - Running on Pi 5 with Raspberry Pi OS Bookworm 64-bit
#   - Navio2 HAT is physically attached
#   - Kernel 6.6.x (tested: 6.6.51+rpt-rpi-2712)
#   - Internet access for apt packages
#   - Script is in the project root (navio2_rp5/)
# =============================================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
echo "Project root: $SCRIPT_DIR"

# --- 1. RP1 drive-strength service -------------------------------------------
echo "[1/5] Installing RP1 SPI1 12mA drive-strength service..."
cat > /usr/local/bin/rp1-spi1-drive.py << 'PYEOF'
#!/usr/bin/env python3
"""
Set RP1 SPI1 header pins (GPIO16/19/20/21) to 12mA drive + fast slew.
Root cause: RP1 GPIO defaults to 4mA; BCM2711 (Pi 4) uses 16mA.
The Navio2 RCIO STM32F103 needs sufficient drive through the HAT PCB traces.
"""
import mmap, struct, os, sys
GPIOMEM = "/dev/gpiomem0"
PADS_BASE = 0x20000
MAP_SIZE = 256 * 1024
PINS = [16, 19, 20, 21]  # CS, MISO, MOSI, SCLK
fd = os.open(GPIOMEM, os.O_RDWR | os.O_SYNC)
mm = mmap.mmap(fd, MAP_SIZE, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
for pin in PINS:
    off = PADS_BASE + 4 + pin * 4
    mm.seek(off)
    val = struct.unpack("<I", mm.read(4))[0]
    new_val = (val & ~0x70) | 0x70  # DRIVE=12mA(0x30) + SLEWFAST(0x40)
    mm.seek(off)
    mm.write(struct.pack("<I", new_val))
mm.close()
os.close(fd)
print("RP1 SPI1 GPIO16/19/20/21 set to 12mA + fast slew")
sys.exit(0)
PYEOF
chmod +x /usr/local/bin/rp1-spi1-drive.py

cat > /etc/systemd/system/rp1-spi1-drive.service << 'SVCEOF'
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
SVCEOF
systemctl enable rp1-spi1-drive.service
echo "  -> Service installed and enabled"

# --- 2. /boot/firmware/config.txt overlays -----------------------------------
echo "[2/5] Configuring /boot/firmware/config.txt overlays..."
CONFIG=/boot/firmware/config.txt

add_overlay() {
    local line="$1"
    if ! grep -q "^${line}" "$CONFIG"; then
        echo "$line" >> "$CONFIG"
        echo "  -> Added: $line"
    else
        echo "  -> Already present: $line"
    fi
}

# Remove any old gpio-spi overlay if present
sed -i '/dtoverlay=rcio-gpio-spi/d' "$CONFIG"

# Add required overlays in order (only if not already present)
add_overlay "dtoverlay=spi1-1cs,cs0_pin=16,cs0_spidev=disabled"
add_overlay "dtoverlay=rcio-pi5"

# --- 3. RCIO module blacklist ------------------------------------------------
echo "[3/5] Blacklisting RCIO modules for safe boot..."
cat > /etc/modprobe.d/blacklist-rcio.conf << 'BLEOF'
# Navio2 RCIO modules: blacklisted for safe boot.
# Load manually after rp1-spi1-drive.service sets GPIO drive strength.
# Do NOT auto-load: I2C bus contention with MS5611 barometer.
blacklist rcio_core
blacklist rcio_spi
install rcio_core /bin/true
install rcio_spi /bin/true
BLEOF
echo "  -> /etc/modprobe.d/blacklist-rcio.conf written"

# --- 4. Build RCIO kernel modules --------------------------------------------
echo "[4/5] Building RCIO kernel modules..."
BUILD_DIR="$SCRIPT_DIR/rcio_source"
if [ ! -d "$BUILD_DIR" ]; then
    echo "  ERROR: rcio_source/ not found at $BUILD_DIR"
    exit 1
fi

apt-get install -y --quiet build-essential "linux-headers-$(uname -r)" 2>/dev/null || true

cd "$BUILD_DIR"
make clean
make
echo "  -> Built: rcio_core.ko rcio_spi.ko"

# Install to a known location
install -m 644 rcio_core.ko rcio_spi.ko /usr/local/lib/ 2>/dev/null || true
echo "  -> Copied to /usr/local/lib/"

# --- 5. Verification script --------------------------------------------------
echo "[5/5] Creating RCIO load-and-verify script..."
cat > /usr/local/bin/rcio-start.sh << 'VEOF'
#!/bin/bash
# Load Navio2 RCIO modules and verify alive=1
set -e
echo "Loading RCIO modules..."
sudo insmod /usr/local/lib/rcio_core.ko 2>/dev/null || sudo insmod /home/pi/rcio_build/rcio_core.ko
sudo insmod /usr/local/lib/rcio_spi.ko  2>/dev/null || sudo insmod /home/pi/rcio_build/rcio_spi.ko
sleep 2
alive=$(cat /sys/kernel/rcio/status/alive 2>/dev/null || echo "0")
board=$(cat /sys/kernel/rcio/status/board_name 2>/dev/null || echo "unknown")
crc=$(cat /sys/kernel/rcio/status/crc 2>/dev/null || echo "0")
echo "RCIO status: alive=$alive board=$board crc=$crc"
if [ "$alive" = "1" ]; then
    echo "SUCCESS: Navio2 RCIO is working"
else
    echo "FAIL: RCIO not responding"
    exit 1
fi
VEOF
chmod +x /usr/local/bin/rcio-start.sh

echo ""
echo "============================================================"
echo "Setup complete. NEXT STEPS:"
echo "  1. sudo reboot"
echo "  2. After reboot: rcio-start.sh"
echo "  3. Verify: cat /sys/kernel/rcio/status/alive  (expect: 1)"
echo ""
echo "NOTE: RCIO and MS5611 (I2C bus 1) must not run concurrently"
echo "without bus contention management. For ArduPilot integration,"
echo "either load RCIO before ArduPilot, or resolve I2C contention."
echo "============================================================"
