#!/bin/bash

CPS=30
MIN_VALUE=20
MAX_VALUE=60
EMA_SCALE=20

GPOPTS="set terminal x11 1 title \"Signal Plot Analysis\" enhanced font \"arial,6\" noraise"


# Get a new random number
random() {
od -A n -t d -N 2 /dev/urandom
}

# Compute an RPN expression
rpn() {
echo "3k $1 p" | dc
}

calc() {
echo "scale=3; $1" | bc
}

echoerr() {
echo -e "$@" 1>&2
}

START=$(echo | ts %.s)
getTime() {
NOW=$(echo | ts %.s)
TIME=$(calc "$NOW - $START")
}

################################################################################
# Exp Moving Average
################################################################################
EMA=0
ALPHA=0
emaInit() {
ALPHA=$(calc "2.0 / ($1 + 1)")
}

emaUpdate() {
EMA=$(calc "($ALPHA * $1) + ((1 - $ALPHA) * $EMA)")
}

emaReset() {
EMA=0
}

emaValue() {
$EMA
}


################################################################################
# Random Generator
################################################################################

# Configure the random numbers generator
# $1 - MIN value
# $2 - MAX value
# $3 - CPS value
rndSetup() {
MIN=$1
MAX=$(rpn "$2 $1 -")
SLEEP=$(calc "1 / $3")
}

rndUpdate() {
rndSetup $1 $2 $3
emaInit $4
}

CONF_FIFO=$(mktemp -u /tmp/bbqueSignalAlalyzer_XXXXXX)
mkfifo $CONF_FIFO

rndGenerator() {
while true; do
	# Get a new random number
	RN=$(random)
	getTime

	# Compute new value
	VALUE=$(rpn "$MAX $RN * 65536 / $MIN +")

	# Update EMA
	emaUpdate $VALUE

	# Dump metrics
	echo  "$TIME Signal $VALUE EMA $EMA"

	# Wait for next sample
	#sleep $SLEEP;
	read -t $SLEEP <>$CONF_FIFO SETTINGS && rndUpdate $SETTINGS
done
}

################################################################################
# Command line processing
################################################################################
tuneGenerator() {
[ $1 != '-' ] && [ $1 -lt $MAX_VALUE ] && MIN_VALUE=$1
[ $2 != '-' ] && [ $2 -gt $MIN_VALUE ] && MAX_VALUE=$2
[ $3 != '-' ] && CPS=$3
[ $4 != '-' ] && EMA_SCALE=$4
echo "$MIN_VALUE $MAX_VALUE $CPS $EMA_SCALE" >$CONF_FIFO
}


################################################################################
# Initialization
################################################################################

MIN_Y=0
MAX_Y=80

emaInit  $EMA_SCALE
rndSetup $MIN_VALUE $MAX_VALUE $CPS

#rndGenerator
rndGenerator | \
	feedgnuplot \
	--autolegend \
	--lines --points \
	--domain --dataid \
	--xlabel "Samples" \
	--ylabel "Signal" \
	--title "Test Signals Analysis" \
	--stream --xlen 60 \
	--extracmd "$GPOPTS" \
	--geometry 949x233+-5-25 &

# Clean-up all background threads on exit
cleanup() {
	kill $(jobs -p)
	killall feedgnuplot
	rm $CONF_FIFO
}
trap cleanup EXIT

echo -e "\t\t***** Signal Test Analyzer *****"
echo
echo "Tuning parameters:"
echo "min <value> - set the minimum value for the random generator"
echo "max <value> - set the maximum value for the random generator"
echo "cps <value> - set the number of samples per seconds (CPS)"
echo "ema <value> - set the number of samples for the Exponential Moving Average (EMA)"
echo
echo

# This is required to properly capture the trap previously defined
while [ 1 ]; do
	read -p "$> " CMD
	tuneGenerator $CMD
done
