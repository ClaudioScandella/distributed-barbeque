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

round() {
echo "scale=0; $1 / 1" | bc
}


echoerr() {
echo -e "$@" 1>&2
}

START=$(echo | ts %.s)
getTime() {
NOW=$(echo | ts %.s)
calc "$NOW - $START"
}


################################################################################
# Distribution Generators
################################################################################

# Generate a uniform distributed number with defined
# $1 - minimum
# $2 - maximum
uniform() {
RN=$(random)
calc "(($2 - $1) * $RN / 65536) + $1"
}

# Test the uniform distribution with the specified:
# $1 - min
# $2 - max
# $3 - number of samples
testUniform() {
unset samples
LOWER_BOUND=$(calc "")
for i in `seq 1 $3`; do
	SAMPLE=$(uniform $1 $2)
	INDEX=$(round "1000 * $SAMPLE")
	let samples[$INDEX]++
done
for o in ${!samples[*]}; do
	SAMPLE=$(calc "$o / 1000")
	printf "%.3f %5d\n" $SAMPLE ${samples[o]}
done
}

testUniformPlot() {
testUniform $1 $2 $3 | \
	feedgnuplot \
	--nolines --points \
	--domain --nodataid \
	--xlabel "Samples" \
	--ylabel "Occurrences" \
	--title "Uniform Distribution Samples"
}

# Get a normally distributed number with defined
# $1 - mean
# $2 - stddev
# Ref: http://www.protonfish.com/random.shtml
normal() {
R1=$(uniform -1 1)
R2=$(uniform -1 1)
R3=$(uniform -1 1)
N11=$(calc "3 + (($R1 * 2) - 1) + (($R2 * 2) - 1) + (($R3 * 2) - 1)")
calc "$1 + ($N11 * $2)"
}


# Test the normal distribution with the specified:
# $1 - mean
# $2 - stddev
# $3 - number of samples
testNormal() {
unset samples
LOWER_BOUND=$(calc "")
for i in `seq 1 $3`; do
	SAMPLE=$(normal $1 $2)
	INDEX=$(round "1000 * $SAMPLE")
	let samples[$INDEX]++
done
for o in ${!samples[*]}; do
	SAMPLE=$(calc "$o / 1000")
	stdbuf -oL printf "%.3f %5d\n" $SAMPLE ${samples[o]}
done
}

testNormalPlot() {
testNormal $1 $2 $3 | \
	feedgnuplot \
	--nolines --points \
	--domain --nodataid \
	--stream \
	--xlabel "Samples" \
	--ylabel "Occurrences" \
	--title "Normal Distribution Samples"
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
# $4 - RRT value
rndSetup() {
MIN=$1
MAX=$(rpn "$2 $1 -")
SLEEP=$(calc "1 / $3")
GGTRLD=$4
}

rndUpdate() {
rndSetup $1 $2 $3 $5
emaInit $4
}

RNDCFG=$(mktemp -u /tmp/bbqueSignalAlalyzer_rndcfg_XXXXXX)
mkfifo $RNDCFG

################################################################################
# Custom Samplers for GoalGap Variance
################################################################################

PGG=0

# Compute GG variation
# $1 - the previous GG value
# $2 - the current GG value
# return: (Variation, PreviousGG)
ggVarGenerator() {
NGG=$(calc "$MIN + (($MAX * $2) / 65536)")
VAR=$(calc "v=$1-$2; if (v<0) -v else v")
echo "$VAR $2"
}

################################################################################
# A Samples Generator
################################################################################


PLOT_GG=$(mktemp -u /tmp/bbqueSignalAlalyzer_plot_XXXXXX)
mkfifo $PLOT_GG
PLOT_VF=$(mktemp -u /tmp/bbqueSignalAlalyzer_plot_XXXXXX)
mkfifo $PLOT_VF

# A configurable source generator:
# $1 - sampler used to generate numbers
# $2 - samples name
dataSource() {
while true; do

    # Compute a new random value
    RN=$(random)
    V1=$(rpn "$MAX $RN * 65536 / $MIN +")

    RESULT=($($1 $PGG $V1))
    GGVAR=${RESULT[0]}
    PGG=${RESULT[1]}

    # Get current time
    TIME=$(getTime)

    # Update EMA
    emaUpdate $GGVAR

    # Dump metrics
    printf "%9.6f GoalGap %7.3f\n" $TIME $V1 >$PLOT_GG
    printf "%9.6f GGTrld %7.3f $2 %7.3f EMA %7.3f\n" $TIME $GGTRLD $GGVAR $EMA >$PLOT_VF

    # Wait for next sample
    #sleep $SLEEP;
    read -t $SLEEP <>$RNDCFG SETTINGS && rndUpdate $SETTINGS

done
}

ggVarSource() {
dataSource ggVarGenerator "GoalGapVariation"
}

# A configurable plotting function, where:
# $1 - data source
# $2 - Title
# $3 - Y axis label
# $4 - Y max (or - for autoscale)
# $5 - Geometry
plotData() {
[ $4 != "-" ] && YMAX="--ymax=$4"
tail -n0 -f "$1" | \
	feedgnuplot \
	--autolegend \
	--lines --points \
	--domain --dataid \
	--xlabel "Time [s]" \
	--ylabel "$3" $YMAX \
	--title  "$2" \
	--stream --xlen 60 \
	--extracmd "$GPOPTS" \
	--geometry "$5" &
}

################################################################################
# Command line processing
################################################################################
tuneGenerator() {
[ $1 != '-' ] && [ $1 -lt $MAX_VALUE -o $1 -lt $2 ] && MIN_VALUE=$1
[ $2 != '-' ] && [ $2 -gt $MIN_VALUE -o $2 -gt $1 ] && MAX_VALUE=$2
[ $3 != '-' ] && CPS=$3
[ $4 != '-' ] && EMA_SCALE=$4
[ $5 != '-' ] && GGTRLD=$5
echo "$MIN_VALUE $MAX_VALUE $CPS $EMA_SCALE $GGTRLD" >$RNDCFG
}



################################################################################
# Main plotting function
################################################################################

usage() {
echo -e "\t\t***** Signal Test Analyzer *****"
echo
echo "Tuning command: <min> <max> <cps> <ema> <gtr>"
echo "  min - set the minimum value for the random generator"
echo "  max - set the maximum value for the random generator"
echo "  cps - set the number of samples per seconds (CPS)"
echo "  ema - set the number of samples for the Exponential Moving Average (EMA)"
echo "  gtr - set the GoalGap threshold"
echo "Use '-' to keep the previous value"
echo
}

plot() {
# Tuning command memento
usage

# Start data generator
ggVarSource &

# Start data plotters
plotData $PLOT_GG 'Goal Gap (GG) Value' 'Goal Gap' - 949x246+-5+490
plotData $PLOT_VF 'GG Variation and EMA' 'GG Variation' 20 949x233+-5-25

}


################################################################################
# Return if script is just sourced
################################################################################

[[ $_ != $0 ]] && return 2>/dev/null

################################################################################
# Initialization
################################################################################

MIN_Y=0
MAX_Y=80

emaInit  $EMA_SCALE
rndSetup $MIN_VALUE $MAX_VALUE $CPS 5



# Clean-up all background threads on exit
cleanup() {
	kill $(jobs -p)
	killall feedgnuplot
	rm $RNDCFG $PLOT_GG $PLOT_VF
}
trap cleanup EXIT

# Main function
plot

# This is required to properly capture the trap previously defined
while [ 1 ]; do
	read -p "$> " CMD
	[ "x$CMD" != "x" ] && tuneGenerator $CMD
done

