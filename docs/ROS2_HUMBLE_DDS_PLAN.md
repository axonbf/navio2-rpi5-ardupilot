# ROS 2 Humble + ArduPilot native DDS — AS-BUILT (TODO #11)

**Status: ✅ WORKING (2026-08-01).** ArduPilot publishes its full native-DDS `/ap/...` topic set
to ROS 2 Humble via a **serial (PTY) XRCE transport**. Verified with live data (`/ap/clock`,
`/ap/navsat` GPS) locally on the Pi, from the laptop on the LAN, and from the laptop over
**WireGuard** (unicast peer over wg0; local multicast kept).
**Author:** Claude Code (Opus 4.8), 2026-07-31 → completed 2026-08-01 — with user.

## TL;DR — how to use it
- **On the Pi:** `source ~/ros2_env.sh` → `ros2 topic list` (Fast-DDS, `ROS_DOMAIN_ID=0`).
- **Laptop @ home (same LAN):** just works out of the box — the laptop's default Connext RMW
  interoperates with the agent's Fast-DDS via LAN multicast on domain 0. No helper needed.
- **Laptop remote / at the lake (over WireGuard):** `source ~/ros2_boat.sh` → `ros2 topic list`.
  This forces `rmw_fastrtps_cpp` + `~/fastdds_wg.xml` — an **additive** profile: default transports
  (local multicast) + Pi `10.0.0.5` as a unicast `initialPeersList` peer. Local terminal-to-terminal
  discovery AND Pi-over-wg0 discovery both work in this one env.
- Keep the **MAVLink link (UDP 14551)** for params/missions/QGC (tablet `10.0.0.6`, laptop `10.0.0.2`).

---

## ⚠️ The big finding — UDP DDS does NOT work on native Navio2/Linux
AP_DDS has two transports: **UDP** and **serial**. The obvious choice (agent on `udp4 -p 2019`,
`DDS_ENABLE=1`, `DDS_UDP_PORT=2019`, `DDS_IP=127.0.0.1`) **initialises but never sends a byte.**

Root cause (from `libraries/AP_DDS/AP_DDS_Client.cpp::main_loop`):
```cpp
#if AP_DDS_UDP_ENABLED && !AP_NETWORKING_BACKEND_SITL
    if (!is_using_serial) {
        if (AP::network().get_ip_active() == 0) { hal.scheduler->delay(1000); continue; } // spins forever
    }
#endif
```
`get_ip_active()` returns `backend ? backend->activeSettings.ip : 0`. AP_Networking's backends are
**CHIBIOS / PPP / SITL / SITL_TUN only — there is no generic-Linux backend**, so on the native
Navio2 build there is **no backend at all** (confirmed: zero `NET:` boot messages even with
`NET_ENABLE=1`). Therefore `get_ip_active()` is permanently 0 and the UDP DDS loop never reaches
ping/session. `NET_ENABLE` can't help — nothing to activate. (SITL works because it compiles the
check *out* via `AP_NETWORKING_BACKEND_SITL`.)

**The serial transport skips that check** (`if (!is_using_serial)`), so **serial is the only
working AP_DDS transport on Navio2/Linux.** On a same-machine setup we bridge ArduPilot ↔ agent
over a **socat PTY pair** (memory-speed, so no baud limit; XRCE topic set is identical to UDP).

## ⚠️ DDS vendor = Fast-DDS (not CycloneDDS)
The **micro-XRCE-DDS agent is a Fast-DDS program** (baked in, not configurable). It republishes the
`/ap/...` topics on **Fast-DDS**, so **every ROS 2 participant must use Fast-DDS + the agent's
domain (0)** to see them. The earlier CycloneDDS/domain-42 plan was made before the agent existed
and is **abandoned** — `~/ros2_env.sh` now sets `rmw_fastrtps_cpp` + domain 0. (`~/cyclonedds.xml`
is a harmless leftover.)

---

## As-built setup

### Pi — packages (RoboStack, isolated conda env `ros_env`)
Miniforge batch-installed (no `conda init`, `.bashrc` untouched). `ros-humble-ros-base` +
`ros-humble-rmw-cyclonedds-cpp` (unused now). Enter via `source ~/ros2_env.sh`.

### Pi — DDS firmware
`~/ardupilot-master` rebuilt with **`--enable-DDS`** (waf flag is capital `DDS`); needs the
`microxrceddsgen` codegen (built from `ardupilot/Micro-XRCE-DDS-Gen`, needs a JDK) on `PATH`.
`/usr/bin/ardurover` is a **real copy** of `build/navio2/bin/ardurover` (was a symlink into the
build dir — decoupled so rebuilds don't silently change the live binary). Client version = **v2.4.1**.

### Pi — the agent (source-built)
eProsima **Micro-XRCE-DDS-Agent v2.4.2** in `~/microxrce-agent/` (superbuild; had to patch its
`CMakeLists.txt` `_fastdds_tag 2.12.x` → `v2.12.2`, a removed branch). Runs as a **systemd service
in serial mode**.

### Pi — the transport chain (3 systemd units)
1. **`dds-pty.service`** — `socat pty,raw,echo=0,perm=0666,link=/dev/ttyDDS0  pty,raw,echo=0,perm=0666,link=/dev/ttyDDS1`
   (⚠️ **no `-d0`** — invalid flag). Creates the virtual serial pair, world-rw (ArduPilot=root, agent=pi).
2. **`xrce-agent.service`** — `MicroXRCEAgent serial --dev /dev/ttyDDS1 -b 115200`
   (`User=pi`, `LD_LIBRARY_PATH=~/microxrce-agent/lib`, `ROS_DOMAIN_ID=0`, `After/Requires=dds-pty`).
3. **`ardurover.service.d/dds-pty.conf`** drop-in — `After=/Wants=dds-pty.service`.
Both `dds-pty` and `xrce-agent` are **enabled** (auto-start at boot).

### Pi — ArduPilot params + serial mapping
- `/etc/default/ardurover`: `TELEM_DDS="--serial5 /dev/ttyDDS0"`, `ARDUPILOT_OPTS="$TELEM1 $TELEM2 $TELEM_DDS"`.
  ⚠️ **NO inline `#` comments** in this file — `EnvironmentFile` leaks them as stray argv words that
  halt ArduPilot's getopt (this silently dropped `--serial2` and corrupted QGC param sync earlier).
- Params (set in QGC, they're stored — the `--defaults` file can't override existing ones):
  `DDS_ENABLE=1`, `SERIAL5_PROTOCOL=45` (DDS-XRCE), `SERIAL5_BAUD=115`.
- Boot message confirms: **`DDS: Using serial`** (not "Using UDP").

### Telemetry (MAVLink, separate from DDS)
`--serial1 udp:10.0.0.6:14551` (tablet), `--serial2 udp:10.0.0.2:14551` (laptop). serial3=GPS (don't
override it — doing so hijacks the GPS port).

### Laptop
- **Home:** default env (native `/opt/ros/humble`, `RMW=rmw_connextdds`, domain 0) — Connext↔Fast-DDS
  interoperate over LAN multicast. Nothing to configure.
- **Remote/WireGuard:** `~/ros2_boat.sh` → `rmw_fastrtps_cpp` + domain 0 +
  `FASTRTPS_DEFAULT_PROFILES_FILE=~/fastdds_wg.xml`. The XML is **additive** — it keeps default
  transports (local multicast works) and lists the **Pi `10.0.0.5` as a unicast `initialPeersList`
  peer** (WireGuard does not forward multicast, so the cross-wg leg must be unicast). Note:
  `initialPeersList` **replaces** Fast-DDS's default peers, so the SPDP multicast locator
  (`239.255.0.1`) is listed explicitly alongside the Pi — without it, local discovery is lost.

### Pi — agent wg unicast profile (symmetric discovery)
`~/fastdds_wg_agent.xml` gives the agent the **initial peer = laptop `10.0.0.2`** (additive — keeps
multicast + all interfaces, so LAN/local still work; **no** wg0 whitelist, or local access would
break). Wired in via drop-in `xrce-agent.service.d/profile.conf`
(`Environment=FASTRTPS_DEFAULT_PROFILES_FILE=/home/pi/fastdds_wg_agent.xml`). This makes discovery
**symmetric** — either side can initiate over wg0. Verified: local Pi still sees 18 `/ap/` topics,
and the laptop additive path still gets topics + live GPS.

---

## Gamepad / joystick → /ap/joy (QoS bridge)

The `joy_node` publisher uses RELIABLE/VOLATILE QoS, but ArduPilot's `/ap/joy` subscription requires BEST_EFFORT/VOLATILE. A direct `--remap` doesn't work because the QoS doesn't match and DDS drops the data at the subscription endpoint.

**Solution**: `config/ros2-dds/joy_relay.py` — a small rclpy node that:
- Subscribes to `/joy` (RELIABLE, matching joy_node)
- Publishes to `/ap/joy` (BEST_EFFORT/VOLATILE, matching ArduPilot)
- Strips to 4 axes (ArduPilot requires `axes_size >= 4`)
- Remaps axes for skid-steer: `[-gamepad[2], gamepad[0], gamepad[1], gamepad[3]]` (steering inverted)

Run on the Pi alongside joy_node:
```bash
# Terminal 1: gamepad plugged into Pi
ros2 run joy joy_node
# Terminal 2:
source ~/ros2_env.sh && source ~/ap_ws/install/setup.bash
python3 ~/joy_relay.py
```

The gamepad can also be on the laptop — both are in domain 0 and Connext↔Fast-DDS interop over LAN makes `/joy` visible to the Pi.

**DDS discovery note**: `~/ros2_boat.sh` uses an **additive** Fast-DDS profile — `initialPeersList`
lists both the SPDP multicast locator (`239.255.0.1`) and the Pi (`10.0.0.5`) unicast peer, so local
terminal-to-terminal discovery AND Pi-over-wg0 discovery both work in the same env. (Earlier the
profile was wg0-only and disabled local multicast; that created a "wall" where two terminals using
`ros2_boat.sh` could not discover each other. Fixed 2026-08-02 by listing multicast explicitly —
`initialPeersList` *replaces* Fast-DDS's default peers, so it must be listed or local discovery is
silently lost.)

---

## Verification (2026-08-01)
- Pi: agent logs `session established`; `ros2 topic list` shows the full `/ap/*` set; `/ap/clock`
  live, `/ap/navsat` at 5 Hz.
- Laptop @ home (LAN): same topic list + live GPS.
- Laptop **wg0** (proves WireGuard): full topic list + live `/ap/navsat` GPS over the tunnel, plus local multicast.

## Retained decisions (still valid)
- **Humble** (RoboStack has no Jazzy for aarch64; matches the laptop's `/opt/ros/humble`).
- **Keep Raspberry Pi OS** (Navio2/RCIO kernel stack works; Ubuntu would force re-porting it).
- **Native DDS, not MAVROS** (MAVROS setpoint/RC rate-limited — the user's original pain).

## Reboot-robustness — ✅ VERIFIED (2026-08-01)
Survives a clean reboot with **no manual steps**: `dds-pty`, `xrce-agent`, `ardurover` all auto-start
(enabled), the PTYs appear, the agent logs `session established`, and `/ap/*` topics + live
`/ap/navsat` GPS were confirmed from the laptop after the boot. The feared socat/ArduPilot PTY race
did **not** occur (the `After=dds-pty` ordering + the agent's `Restart=` cover it). If it ever does
regress, add an `ExecStartPre` wait-for-`/dev/ttyDDS0` to `ardurover`.

## Notes
- `NET_ENABLE=1` was set while chasing the (dead) UDP path; it's **inert** on Navio2 (no backend) —
  safe to leave or revert to 0.
