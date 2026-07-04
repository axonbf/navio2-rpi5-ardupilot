#!/bin/bash

ARCH=`dpkg --print-architecture`

confirm_guard() {
    echo "Would you like to install missing dependencies (requires sudo)? [y/n]"
    while true; do
        read answer
        case $answer in
            [Yy]* ) return ;;
            [Nn]* ) exit ;;
            * ) echo "Wrong answer" ;;
        esac
    done
}

if [[ "$ARCH" == *"arm"* || "$ARCH" == "aarch64" ]]; then
    if ! dpkg -s libgpiod-dev &> /dev/null; then
        confirm_guard
        sudo apt-get update
        sudo apt-get install libgpiod-dev
    fi
else
    echo "Non-ARM architecture detected. Make sure libgpiod-dev is installed for compilation."
    if ! dpkg -s libgpiod-dev &> /dev/null; then
        echo "libgpiod-dev not found. Install it with: sudo apt-get install libgpiod-dev"
    fi
fi
