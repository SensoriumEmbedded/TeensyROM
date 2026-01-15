#!/bin/bash

COREPATH="$HOME/.arduino15/packages/teensy/hardware/avr/1.59.0/cores/teensy4"

set -e

function upper() {
  cp ./BootLinkerFiles/bootdata.c.upper $COREPATH/bootdata.c
  cp ./BootLinkerFiles/imxrt1062_t41.ld.upper $COREPATH/imxrt1062_t41.ld
  echo "Moved address to upper position. Now please compile Teensy.ino"
}

function orig() {
  cp ./BootLinkerFiles/bootdata.c.orig $COREPATH/bootdata.c
  cp ./BootLinkerFiles/imxrt1062_t41.ld.orig $COREPATH/imxrt1062_t41.ld
  echo "Moved address to original position. Now please compile MinimalBoot.ino"
}

function merge() {
  head -n -2 MinimalBoot.ino.hex > merged.hex
  head -n -2 Teensy.ino.hex >> merged.hex
  tail -2 MinimalBoot.ino.hex >> merged.hex
  echo "Merged upper/lower parts into merged.hex"
}

case "$1" in
  upper)
    upper
    ;;

  orig)
    orig
    ;;

  merge)
    merge
    ;;

  ?|help)
    echo "Usage: $(basename $0) upper|orig|merge"
    exit 1
    ;;
esac
