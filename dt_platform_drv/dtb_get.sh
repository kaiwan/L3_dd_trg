#!/bin/sh

which dtc >/dev/null 2>&1 || {
  echo "Require the Device Tree Compiler - dtc - installed
Tip: the package name is device-tree-compiler"
  exit 1
}

echo "dtc -@ -I fs -O dts /proc/device-tree/ 2>/dev/null > this_sys.dts"
dtc -@ -I fs -O dts /proc/device-tree/ 2>/dev/null > this_sys.dts && {
  echo "Success"
  ls -l this_sys.dts
} || echo "failed!"

which fdtdump >/dev/null 2>&1 || {
  echo "Require the fdtdump util installed"
  exit 1
}
echo "
Via fdtdump"
read -p "Pl enter the system's DTB file location [f.e. /boot/...] : " dtbfile
fdtdump ${dtbfile} > fdtdump_out.txt && {
  echo "Success"
  ls -l fdtdump_out.txt
} || echo "failed!"

# ref: https://stackoverflow.com/a/39931834/779269
# For convenience, can add in your ~/.bashrc:
# dtb2dts() ( dtc -@ -I dtb -O dts -o - "$1" )
# dts2dtb() ( dtc -I dts -O dtb -o - "$1" )
