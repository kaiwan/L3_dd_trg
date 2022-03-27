#!/bin/bash
# ftrcit 
# trace-cmd wrapper script..
[ $(id -u) -ne 0 ] && {
 echo "$0: need to be root"
 exit 1
}

KMOD=dtdemo_platdrv

# Filters
echo ":mod:${KMOD}" >> /sys/kernel/debug/tracing/set_ftrace_filter 
echo "*platform*" >> /sys/kernel/debug/tracing/set_ftrace_filter
echo "of_*" >> /sys/kernel/debug/tracing/set_ftrace_filter

echo "trace-cmd: recording ..."
trace-cmd record -p function_graph -F taskset 01 insmod ./${KMOD}.ko
echo "trace-cmd: report gen..."

trace-cmd report -I -S -l > ftrc_report_$(date +%a_%d_%m_%Y_%H%M).txt
 # -I : do not report hardirq events
 # -S : do not report softirq events
 # -l : This adds a "latency output" format
ls -lh ftrc_report_$(date +%a_%d_%m_%Y_%H%M).txt

echo "Unloading module $KMOD..."
rmmod ${KMOD}

trace-cmd reset   # all trace data lost!
