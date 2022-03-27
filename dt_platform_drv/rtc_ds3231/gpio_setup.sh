#!/bin/bash
# GPIO setup for asserting a hardware interrupt via a GPIO pin on the DS3231
#  Doc: https://www.kernel.org/doc/Documentation/gpio/sysfs.txt
GPIO_PIN=16

echo "[Un]exporting GPIO pin #${GPIO_PIN} now..."
echo ${GPIO_PIN} > /sys/class/gpio/unexport
# First export the required GPIO pin to make it turn up
echo ${GPIO_PIN} > /sys/class/gpio/export

# Set Direction as ‘out’ : o/p   (or ‘in’ for i/p):
sudo sh -c "echo out > /sys/class/gpio/gpio${GPIO_PIN}/direction"
# Set triggering: edge, rising:
sudo sh -c "echo rising > /sys/class/gpio/gpio${GPIO_PIN}/edge"

# Check:
echo "ls -l /sys/class/gpio/gpio${GPIO_PIN}/"
ls -l /sys/class/gpio/gpio${GPIO_PIN}/
echo -n "direction: "
cat /sys/class/gpio/gpio${GPIO_PIN}/direction
echo -n "edge: "
cat /sys/class/gpio/gpio${GPIO_PIN}/edge
