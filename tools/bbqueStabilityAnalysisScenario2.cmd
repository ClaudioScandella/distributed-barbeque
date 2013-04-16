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

scnearyStep 4 "Resources are initially stable"

echo r3p 3 1
scnearyStep 10 "Resource 3 failing 1[s] every 4[s]"

echo r3p 1 3
scnearyStep 10 "Resource 3 failing 3[s] every 4[s]"


press_to_terminate

