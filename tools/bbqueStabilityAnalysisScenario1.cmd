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

scnearyStep() {
TSLOT=$1
sleep 2
echo "STEP: $2" 1>&2
if [ $ITV -eq 1 ]; then
  press_to_continue
else
  sleep $((TSLOT-2))
fi
}

scnearyStep 10

print_title "Make the application unstable with coarse grained GG"
echo gg 20 60 -
echo mode 1 - -
scnearyStep 30

print_title "Enabled GG stability control"
echo mode - 1 -
scnearyStep 30

print_title "Refine GoalGap assertions"
echo gg 20 25 -
scnearyStep 30

print_title "Enable RR stability control"
echo mode - - 1
scnearyStep 30

print_title "Shrink RR stability window"
echo rr 20 - - -
scnearyStep 30

print_title "Redefine GoalGap assertions"
echo gg 75 80 -
scnearyStep 30

print_title "Make the application stable"
echo gg 0 0 -
echo mode 0 - -
scnearyStep
