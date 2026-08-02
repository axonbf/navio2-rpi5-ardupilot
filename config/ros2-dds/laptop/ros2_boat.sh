#!/usr/bin/env bash
# Talk to the boat's ROS 2 (local + over WireGuard).
# Agent = Fast-DDS on domain 0; ~/fastdds_wg.xml is additive — local multicast (239.255.0.1)
# + Pi (10.0.0.5) as a wg0 unicast initial peer. local + cross-wg discovery both work.
#   Usage:  source ~/ros2_boat.sh   ->   ros2 topic list
source /opt/ros/humble/setup.bash
source ~/ap_ws/install/setup.bash 2>/dev/null   # ardupilot_msgs overlay (build ~/ap_ws first)
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export ROS_DOMAIN_ID=0
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/fastdds_wg.xml
unset ROS_DISCOVERY_SERVER CYCLONEDDS_URI
echo "ROS 2 boat env | RMW=$RMW_IMPLEMENTATION | DOMAIN=$ROS_DOMAIN_ID | local multicast + wg0 unicast -> Pi 10.0.0.5"
