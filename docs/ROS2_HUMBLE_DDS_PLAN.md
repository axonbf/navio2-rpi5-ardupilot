# ROS 2 Humble + ArduPilot native DDS — AS-BUILT (TODO #11)

**Status: ✅ WORKING (2026-08-01).** ArduPilot publishes its full native-DDS `/ap/...` topic set
to ROS 2 Humble via a **serial (PTY) XRCE transport**. Verified with live data (`/ap/clock`,
`/ap/navsat` GPS) locally on the Pi, from the laptop on the LAN, and from the laptop over
**WireGuard** (wg0-only, so genuinely over the tunnel).
**Author:** Claude Code (Opus 4.8), 2026-07-31 → completed 2026-08-01 — with user.

## TL;DR — how to use it
- **On the Pi:** `source ~/ros2_env.sh` → `ros2 topic list` (Fast-DDS, `ROS_DOMAIN_ID=0`).
- **Laptop @ home (same LAN):** just works out of the box — the laptop's default Connext RMW
  interoperates with the agent's Fast-DDS via LAN multicast on domain 0. No helper needed.
- **Laptop remote / at the lake (only via WireGuard):** `source ~/ros2_boat.sh` → `ros2 topic list`.
  This forces `rmw_fastrtps_cpp` + `~/fastdds_wg.xml` (wg0-only, unicast peer = Pi `10.0.0.5`).
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
  `FASTRTPS_DEFAULT_PROFILES_FILE=~/fastdds_wg.xml`. The XML whitelists **wg0 only** (`10.0.0.2`) and
  sets the **initial peer = Pi `10.0.0.5`** → unicast discovery over the tunnel (no multicast needed).
  The Pi/agent needed **no change** — it already announces on wg0.

---

## Verification (2026-08-01)
- Pi: agent logs `session established`; `ros2 topic list` shows the full `/ap/*` set; `/ap/clock`
  live, `/ap/navsat` at 5 Hz.
- Laptop @ home (LAN): same topic list + live GPS.
- Laptop **wg0-only** (proves WireGuard): full topic list + live `/ap/navsat` GPS over the tunnel.

## Retained decisions (still valid)
- **Humble** (RoboStack has no Jazzy for aarch64; matches the laptop's `/opt/ros/humble`).
- **Keep Raspberry Pi OS** (Navio2/RCIO kernel stack works; Ubuntu would force re-porting it).
- **Native DDS, not MAVROS** (MAVROS setpoint/RC rate-limited — the user's original pain).

## Loose ends
- **Reboot-robustness:** the socat fix landed *after* the last reboot, so a clean boot hasn't been
  re-verified. Possible race: ArduPilot may open `/dev/ttyDDS0` before socat creates it (DDS thread
  then exits). If a reboot shows no session, add an `ExecStartPre` wait-for-`/dev/ttyDDS0` to
  `ardurover` (or make `dds-pty` report ready only once the link exists).
- `NET_ENABLE=1` was set while chasing the UDP path; it's **inert** on Navio2 (no backend) — safe to
  leave or revert to 0.
