#!/bin/bash

# Global settings
EV_CLASSES="-e block -e fs -e ext4 -e filemap -e vfs -e hda -e scsi"
FN_NOT_TRACE="__do_page_fault" # -n __do_page_fault"
BUFFER_SZ_KB_PCPU=9000

# 'sync' the IO and ftrace it via trace-cmd(1)
run_trace_cmd()
{
#--- CMD 2
CMD2="sync"
FTRC_REPORT2=./blk-mmc-report.sync_cmd.txt
CMD2_RAW_TRC_FILE=cmd2.dat

taskset 01 trace-cmd record ${EV_CLASSES} -v -e *vm* -n ${FN_NOT_TRACE} \
-p function_graph -b ${BUFFER_SZ_KB_PCPU} -i -o ${CMD2_RAW_TRC_FILE} -F ${CMD2}

rm -f ${FTRC_REPORT2}
# -l => latency format
#setup_report_header ${FTRC_REPORT2}
trace-cmd report -l -i ${CMD2_RAW_TRC_FILE} >> ${FTRC_REPORT2}
}

### 'main'
[ $(id -u) -ne 0 ] && {
  echo "$0: must run as root"
  exit 1
}
lsmod |grep -q sbd || {
  echo "$0: sbd driver not loaded?"
  exit 1
}
df |grep -q "^/dev/sblock0p1" || {
  echo "$0: sbd fs not mounted?"
  exit 1
}

sudo dd if=/dev/urandom of=/mnt/tmp/dest bs=1024 count=10
run_trace_cmd
ls -lh ${FTRC_REPORT2}
exit 0
