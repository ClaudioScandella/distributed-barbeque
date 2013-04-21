#!/bin/bash

# Enable interactive mode, i.e. press ENTER to advance a step
ITV=${1:-0}

print_title() {
clear 1>&2
echo -e "\n\n\E[1m $1\E[0m\n" 1>&2
}

press_to_continue() {
echo -e "\nPress a key to continue" 1>&2
read KEY
}

press_to_terminate() {
sleep 2
echo -e "\n\nScenary completed, press CTRL+C to terminate" 1>&2
}


STEP=0
scnearyStep() {
TSLOT=$1
sleep 2
printf "STEP %02d: %s" $STEP "$2" 1>&2
if [ $ITV -eq 1 ]; then
  press_to_continue
else
  sleep $((TSLOT-2))
fi
let STEP=STEP+1
}

scnearyStep 5 "Resources are initially stable"

if [ 1 -eq 1  ]; then

echo r2m 2 4 7 9
echo r3m 1 2 4 5
scnearyStep 30 "Unstable resources R2 and R3"

echo r2p 1 0
scnearyStep 30 "Resource 2 now stable"

echo r3p 1 9
scnearyStep 30 "Resource 3 mostly unstable"

echo mode 1
scnearyStep 30 "Resource Availability Filter (RAF) enabeld"

echo r3p 5 5
scnearyStep 30 "Resource 3 half unstable"

echo r3p 9 1
scnearyStep 60 "Resource 3 mostly stable"

echo r3p 5 5
scnearyStep 30 "Resource 3 again half unstable"

echo r3p 1 9
scnearyStep 30 "Resource 3 again mostly unstable"

else

echo r2p 3 3
echo r3p 1 6

fi

press_to_terminate

