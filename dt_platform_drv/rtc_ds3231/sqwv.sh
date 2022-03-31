#!/bin/bash
# Trigger 'square wave' interrupts by toggling the 'value' in GPIO16
# as it causes a rise/fall of the edge...
GPIO=16

# First verify that the GPIO pin is configured correctly
dir=$(cat /sys/class/gpio/gpio${GPIO}/direction)
[ "${dir}" != "out" ] && {
  echo "error: GPIO16 direction isn't 'out' (its ${dir})"
  echo "First run the gpio_setup.sh script!"
  exit 1
}
edge=$(cat /sys/class/gpio/gpio${GPIO}/edge)
[ "${edge}" != "rising" ] && {
  echo "error: GPIO16 direction isn't 'rising' (its ${edge})"
  echo "First run the gpio_setup.sh script!"
  exit 1
}

i=0
while [ $i -lt 10 ]
do
	sudo sh -c "echo 1 > /sys/class/gpio/gpio${GPIO}/value"
	sleep .1 # ~ 100ms
	sudo sh -c "echo 0 > /sys/class/gpio/gpio${GPIO}/value"
	let i=i+1
done
