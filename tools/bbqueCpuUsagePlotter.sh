#!/bin/bash

CORES=$(grep processor /proc/cpuinfo | wc -l)
MAXF=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)

function readCpuLoad() {
while true; do
	cat /home/derkling/Documents/Coding/MyBOSP/out/mnt/cgroup/cpuacct.usage_percpu
	sleep 1;
done
}

readCpuLoad | \
	stdbuf -oL ./bbqueCpuUsageParser.awk \
		-v CORES=$CORES -v MAXF=$MAXF | \
	feedgnuplot \
	--autolegend \
	--lines --nopoints \
	--curvestyleall "lw 2" \
	--nodomain --dataid \
	--xlabel "Time [s]" \
	--ylabel "CPU usage [%]" \
	--title "CPU usage (by HW thread)" \
	--extracmd "set key outside below; set ytics mirror" \
	--stream --xlen 60 \
	--ymin 0 --ymax 120 \
	--geometry 949x233+-5-25 &


# Clean-up all background threads on exit
function cleanup {
	kill $(jobs -p)
	killall feedgnuplot
}
trap cleanup EXIT

# This is required to properly capture the trap previously defined
while [ 1 ]; do
	sleep 60
	read KEY
done
