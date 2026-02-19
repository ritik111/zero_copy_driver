#!/bin/bash

sudo insmod ./driver/my_device.ko
sudo chmod 666 /dev/my_device

echo "Driver loaded and permissions set."
