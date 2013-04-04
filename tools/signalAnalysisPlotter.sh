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
S_RC_RUN_TIME=20   # The [ms] AVERAGE for onRun
S_RC_RUN_STDV=2    # The [ms] STDDEV  for onRun
S_RC_MON_TIME=5    # The [ms] AVERAGE for onMonitor
S_RC_MON_STDV=1    # The [ms] STDDEV  for onMonitor
S_RC_RCF_TIME=300  # The [ms] AVERAGE for onConfigure
S_RC_RCF_STDV=50   # The [ms] STDDEV  for onConfigure
S_RR_EMA_SMPLS=60  # RR EMA Samples number
S_RR_EMA_ALPHA=0   # RR EMA Alpha value
S_PTME=0     # Previous Cycle Time
S_CTME=0     # Current Cycle Time
S_RTME=0     # Last Reconfiguration request Time
S_UTME=500   # The [ms] from a switchUp reconfiguration
S_DTME=10    # The [ms] from a switchDown reconfiguration
S_PAWM=1     # Previous AWM
S_CAWM=1     # Current  AWM
S_SM_CAPP=0  # Application control: 0 stable, 1 unstable
S_SM_CLIB=0  # RTLib control: 0 disabled, 1 enabled
S_SM_CBBQ=0  # BBQ control: 0 disabled, 1 enabled
# Model Metrics
M_GGVAR=0    # GoalGap Variation
M_GGEMA=0    # GoalGap EMA
M_GGTRD=5    # GoalGap Threshold
M_GGPRV=0    # GoalGap Previous
M_GGCUR=3    # GoalGap Previous
M_RRCUR=0    # Reconfiguration Rate Current
M_RREMA=0    # Reconfiguration Rate EMA
M_RRTRD=5    # Reconfiguration Rate Threshold

# Model initialization
S_CPS_SLEEP=$(calc "1 / $S_CPS")
S_GG_EMA_ALPHA=$(emaInit  $S_GG_EMA_SMPLS)
S_RR_EMA_ALPHA=$(emaInit  $S_RR_EMA_SMPLS)

# Simulation mode string representation
S_SM_APP[0]="Stable"
S_SM_APP[1]="Unstable"
S_SM_CTR[0]="Disabled"
S_SM_CTR[1]="Enabled"

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
# Data Generator Configuration
################################################################################

# Report current configuration
cfgReport() {
echo
echo "*** Current configuration"
echo "GoalGap:"
echo "  min............. $S_RND_GG_MIN"
echo "  max............. $S_RND_GG_MAX"
echo "  ema_samples..... $S_GG_EMA_SMPLS"
echo "CPS............... $S_CPS"
echo "App............... ${S_SM_APP[$S_SM_CAPP]}"
echo "RTLib control..... ${S_SM_CTR[$S_SM_CLIB]}"
echo "BBQ control....... ${S_SM_CTR[$S_SM_CBBQ]}"
echo
}

# Configure the random numbers generator
cfgUpdate() {
case $1 in

0) # Goal Gap Configuration
S_RND_GG_MIN=$2
S_RND_GG_MAX=$3
S_RND_GG_DLT=$(calc "$3 - $2")
S_GG_EMA_SMPLS=$4
S_GG_EMA_ALPHA=$(emaInit  $4)
;;

1) # CPS Configuration
S_CPS=$2
S_CPS_SLEEP=$(calc "1 / $2")
;;

2) # Simulation Mode
S_SM_CAPP=$2
S_SM_CLIB=$3
S_SM_CBBQ=$4
;;

esac

# Dump current configuration
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
[ -n "$1" ] && RND=$1 || RND=$(random)
M_GGCUR=$(calc "$S_RND_GG_MIN + (($S_RND_GG_DLT * $RND) / 65536)")
M_GGVAR=$(calc "v=$M_GGCUR-$M_GGPRV; if (v<0) -v else v")
M_GGPRV=$M_GGCUR
}

################################################################################
# AWM Assignement Model
################################################################################

updateReconfTime() {
REC=$(calc "if ($S_RTME > 0) 0 else 1")
[ $REC -eq 0 ] && return

DTIME=$S_UTME
[ $S_CAWM -eq 3 ] && DTIME=$S_DTME

S_RTME=$(calc "$S_CTME + ($DTIME / 1000)")
#printf "%9.6f Reconfigure scheduled @ $S_RTME\n" $S_CTME
}

# Check if the current AWM should be switched DOWN
downAWM() {
# => compute GG
ggVarGenerator
#  set reconfiguration time
updateReconfTime
}

# Check if the current AWM should be switched UP
upAWM() {
# => GG = 0;
ggVarGenerator 0
# set reconfiguration time
updateReconfTime
}

# Update the Run-Time Costs
updateRR() {
RT=$(normal "$S_RC_RUN_TIME" "$S_RC_RUN_STDV")
MT=$(normal "$S_RC_MON_TIME" "$S_RC_MON_STDV")
if [ $S_PAWM != $S_CAWM ]; then
	CT=$(normal "$S_RC_RCF_TIME" "$S_RC_RCF_STDV")
else
	CT=0
fi
M_RRCUR=$(calc "(($MT + $CT)/1000) / ($RT/1000)")

# Update RR EMA
M_RREMA=$(emaUpdate $S_RR_EMA_ALPHA $M_RREMA $M_RRCUR)
}

# Switch the current AWM, if required
# This function simulate the BBQ policy
switchAWM() {
SWITCH_TIME=$(calc "if(($S_RTME>0) && ($S_RTME<=$S_CTME)) 1 else 0")
[ $SWITCH_TIME -eq 0 ] && return

NAWM=1
[ $S_CAWM -eq 1 ] && NAWM=3

#printf "%9.6f Switch AWM: $S_CAWM => $NAWM\n" $S_CTME
S_PAWM=$S_CAWM
S_CAWM=$NAWM
S_RTME=0
}

# Update the current AWM model, where:
updateAWM() {

# Check if current AWM must be changed
switchAWM

# Current AWM = High
[ $S_CAWM -eq 3 ] && downAWM

# Current AWM = Low
[ $S_CAWM -eq 1 ] && upAWM

# Update EMA
M_GGEMA=$(emaUpdate $S_GG_EMA_ALPHA $M_GGEMA $M_GGVAR)

updateRR

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
PLOT_RR=$(mktemp -u /tmp/bbqueSignalAlalyzer_plotRRFilter_XXXXXX)
mkfifo $PLOT_RR

# The data generator
dataSource() {
while true; do

    # Get current time
    S_CTME=$(getTime)

    # Update AWM Model
    updateAWM

    # Dump metrics
    printf "%9.6f CurrAWM %2d\n" $S_CTME $S_CAWM >$PLOT_AWM
    printf "%9.6f GoalGap %7.3f\n" $S_CTME $M_GGCUR >$PLOT_GG
    printf "%9.6f GG_Thr %7.3f GG_Var %7.3f GG_EMA %7.3f\n" $S_CTME $M_GGTRD $M_GGVAR $M_GGEMA >$PLOT_VF
    printf "%9.6f RR_Thr %7.3f RR_Cur %7.3f RR_EMA %7.3f\n" $S_CTME $M_RRTRD $M_RRCUR $M_RREMA >$PLOT_RR

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
	--stream --xlen 90 \
	--extracmd "$GPOPTS" \
	--geometry "$5" &
}

################################################################################
# Command line processing
################################################################################

tuneGenerator() {
case $1 in

"gg") # Goal Gap Configuration
[ $2 != '-' ] && [ $2 -lt $S_RND_GG_MAX -o $2 -lt $3 ] && S_RND_GG_MIN=$2
[ $3 != '-' ] && [ $3 -gt $S_RND_GG_MIN -o $3 -gt $2 ] && S_RND_GG_MAX=$3
[ $4 != '-' ] && S_GG_EMA_SMPLS=$4
echo "0 $S_RND_GG_MIN $S_RND_GG_MAX $S_GG_EMA_SMPLS" >$CFGFIFO
;;

"cps") # CPS Configuration
[ $2 != '-' ] && S_CPS=$2
echo "1 $S_CPS" >$CFGFIFO
;;

"mode") # Model Mode
[ $2 != '-' ] && S_SM_CAPP=$2
[ $3 != '-' ] && S_SM_CLIB=$3
[ $4 != '-' ] && S_SM_CBBQ=$4
echo "2 $S_SM_CAPP $S_SM_CLIB $S_SM_CBBQ" >$CFGFIFO
;;

esac

}


################################################################################
# Main plotting function
################################################################################

usage() {
echo -e "\t\t***** Signal Test Analyzer *****"
echo
echo "Tuning commands:"
echo " gg <min> <max> <ema>"
echo "    min  - set the minimum value for the GG random generator"
echo "    max  - set the maximum value for the GG random generator"
echo "    ema  - set the number of samples for the GG Exponential Moving Average (EMA)"
echo " cps <cps>"
echo "    cps  - set the number of samples per seconds (CPS)"
echo " mode <app> <lib> <bbq>"
echo "    app - Application control mode - 0: stable,   1: unstable"
echo "    lib - RTLib control mode       - 0: disabled, 1: enabled"
echo "    bbq - BBQ control mode         - 0: disabled, 1: enabled"
echo
echo "Use '-' to keep the previous value"
echo
}

plot() {
# Tuning command memento and current configuration dump
usage
cfgReport
# Start data plotters
plotData $PLOT_AWM 'Current AWM (GG) value' 'AWM Value' 4 949x233+-5+-8
plotData $PLOT_GG 'Goal Gap (GG) Value' 'Goal Gap' - 949x233+-5+255
plotData $PLOT_VF 'Goal Gap Filter' 'GG Variation' 20 949x233+-5+517
plotData $PLOT_RR 'Reconfiguration Rate Filter' 'RR Variation' 20 949x233+-5-25
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
	rm $CFGFIFO $PLOT_GG $PLOT_VF $PLOT_RR
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
	sleep .2
done

