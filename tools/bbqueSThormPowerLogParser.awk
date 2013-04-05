#!/usr/bin/awk -f

###############################################################################
# Scheduler Metrics
###############################################################################
#
# Log format:
#
# 04-05 16:29:39.210   543   544 V bq.pp   : PWR_STATS: 1367 1307 1500 128
#
# NOTE: these loglines are generated using the 'threadtime' option, i.e.
# adb logcat -v threadtime

BEGIN {
	FS=" "
	start_time = 0.0;
}

/ PWR_STATS: / {

	# Get the timestamp
	timestr=$2
	split(timestr, f, ".");
	timestr=sprintf("2013 %s %s", $1, f[1])
	gsub(/[-\:]/, " ", timestr);
	event_time = mktime(timestr);
	timestamp = event_time + f[2]/1000

	# Keep track of time origin
	if (start_time == 0.0)
		start_time = timestamp;
	timestamp = timestamp - start_time;

	printf("%8.3f PROF_PWR Sampled %4d Average %4d Budget %4d Available %4d\n",
	       timestamp, $9, $10, $11, $12);
}

{
	# Discard all lines
}
