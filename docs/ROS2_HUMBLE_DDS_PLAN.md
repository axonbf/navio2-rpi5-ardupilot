# ROS 2 Humble + ArduPilot native DDS — plan (TODO #11)

**Status:** **Step 1 DONE on the Pi (2026-08-01)** — ROS 2 Humble installed & verified.
Steps 3–4 (agent + CycloneDDS) can be prepped now; **Step 2 (firmware rebuild) waits for
AFTER the lake test**.
**Author:** Claude Code (Opus 4.8), 2026-07-31 / updated 2026-08-01 — decided with user.
**Goal:** native ROS 2 on the Pi 5, ArduPilot as a first-class ROS 2 node, and
`ros2 topic list` / `echo` from the laptop **over WireGuard** (10.0.0.2 ↔ 10.0.0.5).

---

## Decisions (the "why", so the plan isn't just actions)

- **Distro = Humble, not Jazzy** (changed 2026-08-01).
  RoboStack ships **no Jazzy build for the Pi's arch (linux-aarch64)** — zero `ros-jazzy-*`
  packages; only `ros-humble-*` exists for aarch64. Jazzy on the Pi would need a source build
  (hours) or Docker (rejected, see below). **Humble is available now AND matches the laptop's
  native `/opt/ros/humble`** → perfect message-definition match, cleanest `ros2 topic echo`,
  no cross-distro mismatch. For this project Jazzy's only extras (service introspection,
  Python 3.12, +2 yr support to 2029) are nice-to-haves we don't need. ArduPilot AP_DDS
  supports Humble fully.

- **Keep Raspberry Pi OS (Debian 12 Bookworm) — do NOT reflash to Ubuntu.**
  Ubuntu would make ROS 2 easy **but** the whole Navio2 stack — the hand-built **RCIO kernel
  module** (6.12.93), the SPI/I2C/PPM **device-tree overlays**, RP1 bits, `config.txt` overlays,
  and the **PWM-refresh fix** — is Pi-OS-kernel-specific and already **working**. Switching to
  Ubuntu would force re-porting + re-validating that whole hardware stack (the hard part, already
  solved). Net: Pi OS is *better* for Navio2.

- **Install vehicle = RoboStack (conda), not official apt, not source build, not Docker.**
  - Official `apt install ros-humble-*` (→ `/opt/ros/humble`) is **Ubuntu-only** — no Debian repo.
  - Source build is heavy (multi-hour compile, incomplete Debian `rosdep`, rebuild on every
    update). The `/opt/ros` layout isn't functionally needed.
  - **Docker** rejected: its NAT bridge **breaks DDS multicast discovery**, working against the
    cross-machine-over-WireGuard goal. Native networking is the point.
  - **RoboStack** = ROS 2 packaged as conda packages → native ROS 2 Humble on Debian arm64,
    isolated in an env, reversible. Large curated package set; anything missing is **built into
    an overlay workspace on top** (so coverage is never a dead-end).
  - Installed **batch-mode without `conda init`** → `.bashrc` untouched, so the boat's normal
    shells and the `ardurover` systemd service are completely unaffected. Enter the env with
    `source ~/miniforge3/etc/profile.d/conda.sh && conda activate ros_env`.

- **Integration = native DDS (AP_DDS) + micro-ROS agent, not MAVROS.**
  MAVROS is a MAVLink→ROS *translation* layer; its setpoint/RC streaming is rate-limited
  (the old high-frequency-command pain). Native DDS makes ArduPilot a real ROS 2 participant —
  lower latency, proper QoS, designed for ROS-native control rates. Tradeoff: DDS exposes a
  curated (growing) subset vs MAVLink's full surface, so **keep the MAVLink link (UDP 14551) for
  params/missions/QGC**. The **micro-ROS agent** is the required bridge that lifts ArduPilot's
  XRCE-DDS client onto the full ROS 2 graph.

- **Current `ardurover` build has NO DDS** (binary scan 2026-07-31 found no `AP_DDS`) →
  Step 2 is a firmware rebuild with `--enable-dds`.

- **`ros2 topic list` over WireGuard needs unicast DDS discovery.**
  Default DDS discovery is **multicast**; WireGuard (point-to-point) doesn't pass multicast →
  use **CycloneDDS with a static unicast peer list** (10.0.0.5 ↔ 10.0.0.2) + shared `ROS_DOMAIN_ID`.
  Both ends must use the **same RMW** (CycloneDDS on Pi *and* laptop).

---

## Steps

| # | Step | What it does | Verify | State |
|---|------|--------------|--------|-------|
| 1 | **RoboStack → ROS 2 Humble on the Pi** | Miniforge (batch, no `.bashrc`); `mamba create -n ros_env`; channels conda-forge + robostack-staging; `mamba install ros-humble-ros-base`. | `ros2` CLI + DDS pub/sub loopback | ✅ **DONE 2026-08-01** |
| 2 | **Rebuild `ardurover` with native DDS** | In `~/ardupilot-master`: `./waf configure --board=navio2 --toolchain=native --enable-dds` → `./waf rover`. Set `DDS_ENABLE=1` (+ transport/port). | `strings .../bin/ardurover \| grep -i AP_DDS` non-empty; boat still arms/drives | 🚤 **after lake** |
| 3 | **micro-ROS agent on the Pi** | Install/run the XRCE-DDS agent bridging ArduPilot → ROS 2 graph (RoboStack pkg or source build). | agent launches & listens; later `ros2 node list` shows the AP node | prep now |
| 4 | **DDS discovery over WireGuard** | Install `rmw_cyclonedds`; write CycloneDDS XML with static unicast peers `10.0.0.5 ↔ 10.0.0.2`; shared `ROS_DOMAIN_ID`. | local pub/sub over CycloneDDS RMW | prep now |
| 5 | **Laptop side** | Laptop **already has native ROS 2 Humble** (`/opt/ros/humble`) → just add `rmw_cyclonedds` + identical CycloneDDS config + same domain ID. | laptop sees the Pi over WG | small |
| 6 | **Verify end-to-end over WireGuard** | From laptop: `ros2 topic list`, `ros2 topic echo /ap/battery`. Keep MAVLink 14551 for params/missions. | live topics stream laptop←Pi over `wg0` | needs #2 |

**Working rule:** build + test each step in isolation before moving on (incremental-validation
rule). Steps 1/3/4 are isolated conda-env work and do **not** touch the boat; only Step 2 changes
firmware and waits for after the lake test.

## Facts checked
- 2026-07-31 — Disk: 20 GB free; OS Debian 12 Bookworm arm64; no Docker; `ardurover` has no AP_DDS.
- 2026-08-01 — RoboStack has **no Jazzy for aarch64**, only Humble → distro = Humble.
- 2026-08-01 — Laptop is Ubuntu 22.04 with **native ROS 2 Humble already installed** (`/opt/ros/humble`).
- 2026-08-01 — Step 1 verified on the Pi: `ros2` CLI works, DDS pub/sub loopback delivered a message;
  `.bashrc` clean; disk 17 GB free after install.

## Open items to resolve during execution
- micro-ROS agent availability in RoboStack aarch64 (Step 3) — prebuilt vs quick source build.
- Exact CycloneDDS peer/config XML for the WireGuard subnet (Step 4).
- Which `/ap/...` topics this ArduPilot version publishes/subscribes (feature coverage).
