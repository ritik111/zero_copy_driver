#!/bin/bash

MODULE_NAME="my_device"
DEVICE_NODE="/dev/my_device"

echo "Stopping and unloading $MODULE_NAME..."

if lsmod | grep -q "$MODULE_NAME"; then		#Unload the kernel module
    sudo rmmod $MODULE_NAME
    echo "Module $MODULE_NAME removed successfully."
else
    echo "Module $MODULE_NAME is not currently loaded."
fi

if [ -e "$DEVICE_NODE" ]; then		#Optional: Manual cleanup of the device node
    sudo rm "$DEVICE_NODE"
    echo "Device node $DEVICE_NODE removed."
fi

# sudo dmesg -C			# if you want to clear kernel logs for fresh start.

echo "Cleanup complete."
