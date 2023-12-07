#!/bin/bash

set -e  # stop on error
set -u  # ensure variables are defined

# command line parameters
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 <path for output files> [<run_id> [<install path of binary>]]"
  echo "=> If no <run id> or -1, current date used as output files tag"
  echo "=> Set <run id> to -1 if you only want to change the binary path !!"
  exit 1
fi

# Check that we are on the proper node
if [[ "$HOSTNAME" != "en09" && "$HOSTNAME" != "en11" ]]; then
  echo "The readout of the accelerator timing events can only be done on either en11 or en09"
  exit 1
fi

# Check whether the daemon service is running
if [[ `systemctl is-active saftbus.service` != "active" ]]; then
  echo "Saftbus daemon service is not running, readout cannot start"
  echo "=> Please ask an admin to check the status of the daemon or restart it"
  echo "Hint 1: sudo -k systemctl status saftbus.service"
  echo "Hint 2: sudo -k systemctl start saftbus.service"
  echo "=> Eventual warnings about HW/driver in following lines if any is found"
  # Check whether the board is found on the PCIe express bus
  if [ `lspci -vvv | grep "01:00.0 Class 6800: CERN/ECP/EDU Device 019a (rev 01)"  | wc -l` -ne 1 ]; then
     echo "PEXARIA 5 board not found on the PCIe bus or at least not at the expected address"
     exit 2
  else
    # Check whether the proper driver was loaded for the board
    if [ `lspci -vvv -s 01:00.0 | grep "pcie_wb" | wc -l` -ne 2 ]; then
      echo "Wishbone PCIe driver not assigned to the PEXARIA 5 board or not loaded"
      exit 3
    else
      echo "PEXARIA 5 and its driver properly loaded"
    fi
  fi
  exit 4
fi

TAG=`date -u +"%Y-%m-%d_%H-%M-%S_%Z"`
if [ "$#" -eq 2 ]; then
  if [ $2 -gt 0 ]; then
    TAG=$2
  fi
fi

OUT_FILE=$1/acc_timing_events_${TAG}.txt
ERR_FILE=$1/acc_timing_events_${TAG}_errors.log

INSTALL_DIR=/opt/bel/bin
if [ "$#" -eq 3 ]; then
  INSTALL_DIR=$3
fi
${INSTALL_DIR}/saft-mcbm-ro tr0 1>${OUT_FILE} 2>${ERR_FILE}
