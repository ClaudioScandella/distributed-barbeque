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
emaInit() {
SAMPLES=$1
calc "2.0 / ($SAMPLES + 1)"
}

emaUpdate() {
ALPHA=$1
EMA=$2
NEW=$3
calc "($ALPHA * $NEW) + ((1 - $ALPHA) * $EMA)"
}

################################################################################
# Simulation Model Status and Metrics
################################################################################
# Model Status
S_CPS=20
S_CPS_SLEEP=0      # Sleep time for configured CPS
S_RND_GG_MIN=20    # GG Random Generator Mimimum
S_RND_GG_MAX=60    # GG Random Generator Maximum
S_RND_GG_DLT=40    # GG Random Generator Delta, i.e. Min-Max
S_GG_EMA_SMPLS=60  # GG EMA Samples number
S_GG_EMA_ALPHA=0   # GG EMA Alpha value
S_PTME=0     # Previous Cycle Time
S_CTME=0     # Current Cycle Time
S_CAWM=0     # Current AWM
# Model Metrics
M_GGVAR=0    # GoalGap Variation
M_GGEMA=0    # GoalGap EMA
M_GGTRD=5    # GoalGap Threshold
M_GGPRV=0    # GoalGap Previous
M_GGCUR=3    # GoalGap Previous
M_RREMA=0    # Reconfiguration Rate EMA
M_RRTRD=5    # Reconfiguration Rate Threshold

# Model initialization
S_CPS_SLEEP=$(calc "1 / $S_CPS")
S_GG_EMA_ALPHA=$(emaInit  $S_GG_EMA_SMPLS)

################################################################################
# Working Mode Management
################################################################################

CURR_AWM=1
CURR_GG=0

setLowAWM() {
CURR_AWM=0
}
setHighAWM() {
CURR_AWM=1
CURR_GG=40
}


################################################################################
# Random Generator
################################################################################

# Report current configuration
cfgReport() {
echo "*** Current configuration"
echo "GoalGap($S_RND_GG_MIN,$S_RND_GG_MAX), EMA Samples: $S_GG_EMA_SMPLS"
echo "CPS: $S_CPS"
}

# Configure the random numbers generator
# $1 - GG MIN value
# $2 - GG MAX value
# $3 - GG EMA Samples
# $3 - CPS value
cfgUpdate() {
S_RND_GG_MIN=$1
S_RND_GG_MAX=$2
S_RND_GG_DLT=$(calc "$2 - $1")
S_GG_EMA_SMPLS=$3
S_GG_EMA_ALPHA=$(emaInit  $3)
S_CPS=$4
S_CPS_SLEEP=$(calc "1 / $4")
cfgReport
}

CFGFIFO=$(mktemp -u /tmp/bbqueSignalAlalyzer_rndcfg_XXXXXX)
mkfifo $CFGFIFO

################################################################################
# Custom Samplers for GoalGap Variance
################################################################################

# Compute GG variation
# return: (CurrentGG, VariationGG)
ggVarGenerator() {
RND=$(random)
M_GGCUR=$(calc "$S_RND_GG_MIN + (($S_RND_GG_DLT * $RND) / 65536)")
M_GGVAR=$(calc "v=$M_GGCUR-$M_GGPRV; if (v<0) -v else v")
M_GGPRV=$M_GGCUR
}

################################################################################
# AWM Assignement Model
################################################################################

# Update the current AWM model, where:
updateAWM() {

    #echo "Old AWM Status: $S_PTME, $S_CTME, $S_CAWM; Metrics: $M_GGEMA, $M_GGTRD, $M_RREMA, $M_RRTRD"

    # Compute new GG Variation
    ggVarGenerator

    #echo "New AWM Status: $S_PTME, $S_CTME, $S_CAWM; Metrics: $M_GGEMA, $M_GGTRD, $M_RREMA, $M_RRTRD"

}


################################################################################
# A Samples Generator
################################################################################


PLOT_AWM=$(mktemp -u /tmp/bbqueSignalAlalyzer_plotAWM_XXXXXX)
mkfifo $PLOT_AWM
PLOT_GG=$(mktemp -u /tmp/bbqueSignalAlalyzer_plotGG_XXXXXX)
mkfifo $PLOT_GG
PLOT_VF=$(mktemp -u /tmp/bbqueSignalAlalyzer_plotGGFilter_XXXXXX)
mkfifo $PLOT_VF

# The data generator
dataSource() {
while true; do

    # Get current time
    S_CTME=$(getTime)

    # Update AWM Model
    updateAWM

    # Update EMA
    M_GGEMA=$(emaUpdate $S_GG_EMA_ALPHA $M_GGEMA $M_GGVAR)

    # Dump metrics
    printf "%9.6f CurrAWM %2d\n" $S_CTME $S_CAWM >$PLOT_AWM
    printf "%9.6f GoalGap %7.3f\n" $S_CTME $M_GGCUR >$PLOT_GG
    printf "%9.6f GG_Thr %7.3f GG_Var %7.3f GG_EMA %7.3f\n" $S_CTME $M_GGTRD $M_GGVAR $M_GGEMA >$PLOT_VF

    # Wait for next sample, or update the configuration
    read -t $S_CPS_SLEEP <>$CFGFIFO SETTINGS && cfgUpdate $SETTINGS

done
}

# A configurable plotting function, where:
# $1 - data source
# $2 - Title
# $3 - Y axis label
# $4 - Y max (or - for autoscale)
# $5 - Geometry
plotData() {
YMAX=""
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
[ $1 != '-' ] && [ $1 -lt $S_RND_GG_MAX -o $1 -lt $2 ] && S_RND_GG_MIN=$1
[ $2 != '-' ] && [ $2 -gt $S_RND_GG_MIN -o $2 -gt $1 ] && S_RND_GG_MAX=$2
[ $3 != '-' ] && S_GG_EMA_SMPLS=$3
[ $4 != '-' ] && S_CPS=$4
echo "$S_RND_GG_MIN $S_RND_GG_MAX $S_GG_EMA_SMPLS $S_CPS" >$CFGFIFO
}


################################################################################
# Main plotting function
################################################################################

usage() {
echo -e "\t\t***** Signal Test Analyzer *****"
echo
echo "Tuning command: <gg_min> <gg_max> <gg_ema> <cps>"
echo "  gg_min - set the minimum value for the GG random generator"
echo "  gg_max - set the maximum value for the GG random generator"
echo "  gg_ema - set the number of samples for the GG Exponential Moving Average (EMA)"
echo "  cps    - set the number of samples per seconds (CPS)"
echo "Use '-' to keep the previous value"
echo
}

plot() {
# Tuning command memento and current configuration dump
usage
cfgReport
# Start data plotters
plotData $PLOT_AWM 'Current AWM (GG) value' 'AWM Value' 4 949x259+-5+193
plotData $PLOT_GG 'Goal Gap (GG) Value' 'Goal Gap' - 949x246+-5+490
plotData $PLOT_VF 'GG Variation and EMA' 'GG Variation' 20 949x233+-5-25
# Start data generator
dataSource &
}


################################################################################
# Return on script sourcing or Ctrl+C
################################################################################

# Return if the script has been sources
[[ $_ != $0 ]] && return 2>/dev/null

# Return on Ctrl-+ by cleaning-up all background threads
cleanup() {
	kill $(jobs -p)
	killall feedgnuplot
	rm $CFGFIFO $PLOT_GG $PLOT_VF
}
trap cleanup EXIT


################################################################################
# Initialization
################################################################################

# Main function
plot

# This is required to properly capture the trap previously defined
while [ 1 ]; do
	read -p "$> " CMD
	[ "x$CMD" != "x" ] && tuneGenerator $CMD
done

