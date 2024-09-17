#!/bin/bash
# Batch firmware programming of TouchWheelSAO using arduino-cli
# Requires arduino-cli to be set up as described in "firmware-install.md"

PORT=/dev/ttyUSB0
HEX_FILE=./build/TouchwheelSAO_attiny816.ino.hex

while [ 1 ] ; do
  read -rsn1 -p"Press any key program firmware"; echo
  arduino-cli upload --verbose --fqbn megaTinyCore:megaavr:atxy6:chip=816  \
              --port $PORT --programmer serialupdi57k  \
              --input-file=$HEX_FILE
  echo "--------------------"
done




