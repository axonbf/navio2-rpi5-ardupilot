# ROS 2 Jazzy + ArduPilot native DDS — plan (TODO #11)

**Status:** planned, **run AFTER the lake test** (don't add anything to the boat the night before).
**Author:** Claude Code (Opus 4.8), 2026-07-31 — decided together with user.
**Goal:** full native ROS 2 on the Pi 5, ArduPilot as a first-class ROS 2 node, and
`ros2 topic list` / `echo` from the laptop **over WireGuard** (10.0.0.2 ↔ 10.0.0.5).

---

## Decisions (the "why", so the plan isn't just actions)

- **Keep Raspberry Pi OS (Debian 12 Bookworm) — do NOT reflash to Ubuntu.**
  Ubuntu 24.04 is Jazzy's tier-1 home (easy ROS 2), **but** the whole Navio2 stack —
  the hand-built **RCIO kernel module** (6.12.93), the SPI/I2C/PPM **device-tree overlays**,
  RP1 bits, `config.txt` overlays, and the **PWM-refresh fix** — is Pi-OS-kernel-specific and
  already **working**. Switching to Ubuntu would force re-porting + re-validating that whole
  hardware stack (the hard part, already solved). Net: Pi OS is *better* for Navio2; Ubuntu
  would make ROS 2 easy but Navio2 hard. Not worth it.

- **Install vehicle = RoboStack (conda), not official apt, not source build, not Docker.**
  - Official `apt install ros-jazzy-*` (→ `/opt/ros/jazzy`) is **Ubuntu-only** — no Debian repo.
  - Source build to `/opt/ros/jazzy` is *possible* but heavy (multi-hour compile, incomplete
    Debian `rosdep`, rebuild on every update). The `/opt/ros` layout isn't functionally needed.
  - **Docker** was rejected: its NAT bridge **breaks DDS multicast discovery**, working against
    the cross-machine-over-WireGuard goal. Native networking is the point.
  - **RoboStack** = ROS 2 packaged as conda packages → native ROS 2 Jazzy on Debian arm64,
    ~30 min, isolated in an env, reversible. Large curated package set; anything missing is
    **built into an overlay workspace on top** (so coverage is never a dead-end).

- **Integration = native DDS (AP_DDS) + micro-ROS agent, not MAVROS.**
  MAVROS is a MAVLink→ROS *translation* layer; its setpoint/RC streaming is rate-limited
  (the old high-frequency-command pain). Native DDS makes ArduPilot a real ROS 2 participant —
  lower latency, proper QoS, designed for ROS-native control rates, and where upstream dev is
  focused. Tradeoff: DDS exposes a curated (growing) subset vs MAVLink's full surface, so
  **keep the MAVLink link (UDP 14551) for params/missions/QGC**. The **micro-ROS agent** is the
  required bridge that lifts ArduPilot's XRCE-DDS client onto the full ROS 2 graph.

- **Current `ardurover` build has NO DDS** (binary scan 2026-07-31 found no `AP_DDS`) →
  Step 2 is a firmware rebuild with `--enable-dds`.

- **`ros2 topic list` over WireGuard needs unicast DDS discovery.**
  Default DDS discovery is **multicast**; WireGuard (point-to-point) doesn't pass multicast →
  use **CycloneDDS with a static unicast peer list** (10.0.0.5 ↔ 10.0.0.2) + shared `ROS_DOMAIN_ID`.

---

## Steps

| # | Step | What it does | Verify |
|---|------|--------------|--------|
| 1 | **RoboStack → ROS 2 Jazzy on the Pi** | Install miniforge; `conda create -n ros_env`; `conda install ros-jazzy-ros-base` (headless — boat needs no rviz) + colcon/build tools. | `conda activate ros_env && ros2 --help`; `ros2 doctor` |
| 2 | **Rebuild `ardurover` with native DDS** | In `~/ardupilot-master`: `./waf configure --board=navio2 --toolchain=native --enable-dds` → `./waf rover`. Set params `DDS_ENABLE=1` (+ transport/port). | `strings .../bin/ardurover \| grep -i AP_DDS` non-empty; boat still arms/drives |
| 3 | **micro-ROS agent on the Pi** | Install/run the XRCE-DDS agent bridging ArduPilot → ROS 2 graph. **Early check:** may not be prebuilt in RoboStack → small source build if needed. | agent connects to ardurover; `ros2 node list` shows the AP node |
| 4 | **DDS discovery over WireGuard** | Switch to **CycloneDDS**; static unicast peers `10.0.0.5 ↔ 10.0.0.2`; shared `ROS_DOMAIN_ID`. | local `ros2 topic list` shows `/ap/...` topics |
| 5 | **Laptop side** | ROS 2 Jazzy on laptop (Ubuntu = trivial `apt`; else same RoboStack vehicle) + identical CycloneDDS config + same domain ID. | laptop sees the Pi over WG |
| 6 | **Verify end-to-end over WireGuard** | From laptop: `ros2 topic list`, `ros2 topic echo /ap/battery` (etc.). Keep MAVLink 14551 for params/missions. | live topics stream laptop←Pi over `wg0` |

**Working rule:** build + test each step in isolation on the Pi before moving on
(incremental-validation rule). Nothing here touches the boat before the lake test.

## Facts already checked (2026-07-31)
- Disk: **20 GB free** (29 GB card, 28 % used) → ample for ROS 2 (~3–5 GB).
- OS: **Debian 12 Bookworm, arm64**; no Docker installed.
- Current `ardurover`: **no AP_DDS** compiled in → rebuild required (Step 2).

## Open items to resolve during execution
- micro-ROS agent availability in RoboStack (Step 3) — prebuilt vs quick source build.
- Exact CycloneDDS peer/config file contents for the WireGuard subnet (Step 4).
- Which `/ap/...` topics this ArduPilot version publishes/subscribes (feature coverage).
