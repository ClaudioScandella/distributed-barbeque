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

scnearyStep 4 "Application is initially stable"

echo gg 20 60 -
echo mode 1 - -
scnearyStep 30 "Application is now unstable with coarse grained GG"

echo mode - 1 -
scnearyStep 30 "Enabled GG stability control"

echo gg 20 25 -
scnearyStep 30 "Refined GoalGap assertions to be in range [20,25]"

echo mode - - 1
scnearyStep 30 "Enabled RR stability control"

echo rr 20 - - -
scnearyStep 30 "Reduced RR stability window to 20 samples"

echo rr - - - 20
scnearyStep 30 "Reduce RR stability for higer GG"

echo gg 75 80 -
scnearyStep 30 "Redefined GoalGap assertions to be in range [75,80]"

echo gg 0 0 -
echo mode 0 - -
scnearyStep 30 "Application is now stable again"

