#!/bin/bash

TSLOT=${1:-20}

print_title() {
clear 1>&2
echo -e "\n\n\E[1m $1\E[0m\n" 1>&2
}

press_to_continue() {
echo -e "\nPress a key to continue" 1>&2
read KEY
}

scnearyStep() {
sleep $TSLOT
#press_to_continue
}

sleep 5
scnearyStep

print_title "Make the application unstable with coarse grained GG"
echo gg 20 60 -
echo mode 1 - -
scnearyStep

print_title "Enabled RTLib control"
echo mode - 1 -
scnearyStep

print_title "Refine GoalGap assertions"
echo gg 55 60 -
scnearyStep

print_title "Refine GoalGap assertions"
echo gg 55 80 -
scnearyStep

print_title "Refine GoalGap Variation Filter"
echo gv - - 120
scnearyStep

print_title "Make the application stable"
echo gg 0 0 -
echo mode 0 - -
scnearyStep
