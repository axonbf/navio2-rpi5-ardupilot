# ROS 2 native-DDS config (as deployed)

Version-controlled copies of the working ArduPilot↔ROS 2 native-DDS setup.
Full rationale + the "why" is in [`docs/ROS2_HUMBLE_DDS_PLAN.md`](../../docs/ROS2_HUMBLE_DDS_PLAN.md).

**Architecture:** ArduPilot (XRCE client) —*serial over a socat PTY pair*→ micro-XRCE-DDS agent
(Fast-DDS) —*ROS 2 DDS*→ `/ap/...` topics. UDP DDS does **not** work on Navio2/Linux (no
AP_Networking backend), so the **serial transport** is used. Everything is **Fast-DDS, domain 0**.

> **`ardupilot_msgs` required.** The AP-specific types (`ArmMotors`, `ModeSwitch`, `Status`,
> `GlobalPosition`, …) aren't in a released package — build them into a colcon workspace `~/ap_ws`
> (see `docs/ROS2_HUMBLE_DDS_PLAN.md` / the install notes). **Both helper scripts source it**
> (`source ~/ap_ws/install/setup.bash`). On the **Pi** the overlay must be sourced **after**
> `conda activate ros_env` (RoboStack re-sources base ROS on activate and would wipe an earlier
> overlay). Standard-message topics (`/ap/navsat`, `/ap/cmd_vel`, …) work without it.

## Pi (`pi/`)
| File | Deploy to |
|---|---|
| `ros2_env.sh` | `~/ros2_env.sh` — `source` it to get the ROS 2 env matching the agent |
| `fastdds_wg_agent.xml` | `~/fastdds_wg_agent.xml` — agent's wg unicast peer (laptop `10.0.0.2`) |
| `systemd/dds-pty.service` | `/etc/systemd/system/` — socat PTY pair `/dev/ttyDDS0↔1` |
| `systemd/xrce-agent.service` | `/etc/systemd/system/` — agent in serial mode on `/dev/ttyDDS1` |
| `systemd/xrce-agent.service.d/profile.conf` | drop-in — loads `fastdds_wg_agent.xml` |
| `systemd/ardurover.service.d/dds-pty.conf` | drop-in — order ardurover after the PTY |
| `systemd/ardurover.service.d/wg.conf` | drop-in — order ardurover after wg0 (telemetry boot race) |

Enable: `sudo systemctl daemon-reload && sudo systemctl enable --now dds-pty xrce-agent`.

**Not in files (set via QGC → stored in EEPROM; the `--defaults` file can't override existing params):**
`DDS_ENABLE=1`, `SERIAL5_PROTOCOL=45` (DDS-XRCE), `SERIAL5_BAUD=115`.

**`/etc/default/ardurover`** (⚠️ NO inline `#` comments — they leak as stray argv and break parsing):
```
TELEM1="--serial1 udp:10.0.0.6:14551"     # tablet   (put comments on their OWN line only)
TELEM2="--serial2 udp:10.0.0.2:14551"     # laptop
TELEM_DDS="--serial5 /dev/ttyDDS0"
ARDUPILOT_OPTS="$TELEM1 $TELEM2 $TELEM_DDS"
```

## Laptop (`laptop/`)
| File | Deploy to |
|---|---|
| `ros2_boat.sh` | `~/ros2_boat.sh` — `source` for **remote** (WireGuard) access |
| `fastdds_wg.xml` | `~/fastdds_wg.xml` — wg0-only, unicast peer = Pi `10.0.0.5` |

At **home** (same LAN) nothing is needed — the laptop's default RMW interoperates with the agent's
Fast-DDS over LAN multicast, domain 0. `ros2_boat.sh` is for **remote/lake** over WireGuard.

## WireGuard IPs used
Pi = `10.0.0.5`, laptop = `10.0.0.2`, tablet = `10.0.0.6`. Adjust the `.xml` peers if these change.
