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
