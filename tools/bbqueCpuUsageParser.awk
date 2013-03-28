#!/usr/bin/awk -f

BEGIN {

	for (cpuId = 0; cpuId < CORES; ++cpuId) {
		delta[cpuId]=0;
		current[pudId]=0;
	}

}


function dumpLoad() {

	# Let assume we read one every 1[s],
	# ... and /100 to report percentages
	ns = 10000000

	for (cpuId = 0; cpuId < CORES; ++cpuId)
		delta[cpuId] = current[cpuId] - delta[cpuId];

	data=""
	for (cpuId = 0; cpuId < CORES; ++cpuId) {
		load = delta[cpuId]/ns;
		data = sprintf("%s CPU-%02d %3d", data, cpuId, load);
	}
	printf("%s\n", data);
}

function keepOlds() {
	for (cpuId = 0; cpuId < CORES; ++cpuId)
		delta[cpuId] = current[cpuId];
}

{

	# Get new cycles count
	for (cpuId = 0; cpuId < CORES; ++cpuId)
		current[cpuId] = $(cpuId+1);

	# Compute new load
	if (delta[0] != 0)
		dumpLoad();

	# Update deltas for next computation
	keepOlds();

}
