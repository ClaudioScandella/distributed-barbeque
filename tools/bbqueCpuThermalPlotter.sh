#!/bin/bash

PLOT_TYPE=${1:-"dump"}
CORES=$(grep processor /proc/cpuinfo | wc -l)
MAXF=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
MDEV_NUM_CPU="3"

readCpuTemp() {
while [ true ]; do
	TEMPS=$(sensors | grep Core | awk '{print "Core" $2 " " $3}' | grep -o "Core[0-9]:[\ ]\{1,\}+[0-9]\{1,\}\.[0-9]" | tr -d ":+" | tr '\n' ' ')
	echo $TEMPS
	sleep 1
done
}

if [ "x$PLOT_TYPE" == "xplot" ]; then
	readCpuTemp | \
		stdbuf -oL tee temps.log | \
		feedgnuplot \
		--autolegend \
		--lines --nopoints \
		--curvestyleall "lw 2" \
		--nodomain --dataid \
		--xlabel "Time [s]" \
		--ylabel "CPU usage [%]" \			--title "CPU usage (by HW thread)" \
		--extracmd "set key outside below; set ytics mirror" \
		--stream --xlen 600 \
		--ymin 40 --ymax 100 &
fi

if [ "x$PLOT_TYPE" == "xdump" ]; then
	readCpuTemp | stdbuf -oL tee temps.log
fi

if [ "x$PLOT_TYPE" == "xpng" ]; then
	set -x
	cat temps.log | \
	feedgnuplot \
		--autolegend \
		--lines --nopoints \
		--curvestyleall "lw 2" \
		--nodomain --dataid \
		--xlabel "Time [s]" \
		--ylabel "CPU usage [%]" \
		--title "CPU usage (by HW thread)" \
		--extracmd "set key outside below; set ytics mirror" \
		--ymin 50 --ymax 90 \
		--hardcopy plot.png
	set +x
	exit 0
fi

echo "Plot type: " $PLOT_TYPE
exit 0

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
