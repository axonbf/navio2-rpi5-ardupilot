#!/usr/bin/env bash
# Enter the ROS 2 Humble environment that MATCHES the micro-XRCE-DDS agent.
# The agent bridges ArduPilot onto Fast-DDS on domain 0, so ros2 must use the
# same DDS vendor + domain to see the /ap/... topics.
#   Usage:  source ~/ros2_env.sh
source ~/miniforge3/etc/profile.d/conda.sh
conda activate ros_env
# ardupilot_msgs overlay (colcon ws ~/ap_ws) — MUST come AFTER conda activate,
# or RoboStack's activation re-sources base ROS and wipes the overlay.
source ~/ap_ws/install/setup.bash 2>/dev/null
export ROS_DOMAIN_ID=0                         # must match the xrce-agent service
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp      # match the agent's DDS (Fast-DDS)
unset CYCLONEDDS_URI                            # not used with Fast-DDS
echo "ROS 2 $ROS_DISTRO | RMW=$RMW_IMPLEMENTATION | DOMAIN=$ROS_DOMAIN_ID (matches agent)"
