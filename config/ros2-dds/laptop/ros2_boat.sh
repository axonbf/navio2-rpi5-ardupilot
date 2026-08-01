#!/usr/bin/env bash
# Talk to the boat's ROS 2 over WireGuard.
# Agent = Fast-DDS on domain 0; this forces wg0-only unicast discovery to the Pi.
#   Usage:  source ~/ros2_boat.sh   ->   ros2 topic list
source /opt/ros/humble/setup.bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export ROS_DOMAIN_ID=0
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/fastdds_wg.xml
unset ROS_DISCOVERY_SERVER CYCLONEDDS_URI
echo "ROS 2 boat env | RMW=$RMW_IMPLEMENTATION | DOMAIN=$ROS_DOMAIN_ID | wg0 unicast -> Pi 10.0.0.5"
