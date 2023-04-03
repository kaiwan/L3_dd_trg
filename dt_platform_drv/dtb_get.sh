#!/bin/sh

which dtc >/dev/null 2>&1 || {
  echo "Require the Device Tree Compiler - dtc - installed"
  exit 1
}

echo "dtc -I fs -O dts /proc/device-tree/ 2>/dev/null > this_sys.dts"
dtc -I fs -O dts /proc/device-tree/ 2>/dev/null > this_sys.dts

which fdtdump >/dev/null 2>&1 || {
  echo "Require the fdtdump util installed"
  exit 1
}
echo "Via fdtdump"
read -p "DTB file location [f.e. /boot/...] : " dtbfile
fdtdump dtbfile > fdtdump_out.txt

# ref: https://stackoverflow.com/a/39931834/779269
# For convenience, can add in your ~/.bashrc:
# dtbs() ( dtc -I dtb -O dts -o - "$1" )
# dtsb() ( dtc -I dts -O dtb -o - "$1" )
