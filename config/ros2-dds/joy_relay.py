#!/usr/bin/env python3
"""
Relay /joy to /ap/joy with matching QoS (BEST_EFFORT / VOLATILE)
and exactly 4 axes (what ArduPilot expects).

The joy_node publisher defaults to RELIABLE/TRANSIENT_LOCAL QoS,
which doesn't match ArduPilot's BEST_EFFORT/VOLATILE subscription.
This relay fixes the QoS mismatch and strips extra axes.

Usage:
    ros2 run joy joy_node
    python3 joy_relay.py

Axes mapping (adjust indices for your gamepad):
    relay.axes = [axes[0], axes[1], axes[2], axes[3]]
    ch1=roll, ch2=pitch, ch3=throttle, ch4=yaw (matches RCMAP)
"""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy
from sensor_msgs.msg import Joy


class JoyRelay(Node):
    def __init__(self):
        super().__init__('joy_relay')
        qos = QoSProfile(
            depth=5,
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
        )
        self.pub = self.create_publisher(Joy, '/ap/joy', qos)
        # Subscribe with default QoS (RELIABLE/TRANSIENT_LOCAL) to match joy_node
        self.sub = self.create_subscription(Joy, '/joy', self.callback, 10)

    def callback(self, msg):
        self.get_logger().info(f'Relay: {len(msg.axes)} axes, axes={[f"{a:.2f}" for a in msg.axes[:4]]}')
        relay = Joy()
        relay.header = msg.header
        if len(msg.axes) >= 4:
            relay.axes = [-msg.axes[2], msg.axes[0], msg.axes[1], msg.axes[3]]
        else:
            self.get_logger().warn(
                f'joy_relay: got only {len(msg.axes)} axes, need >=4, ignoring'
            )
            return
        relay.buttons = []
        self.pub.publish(relay)


def main():
    rclpy.init()
    node = JoyRelay()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()