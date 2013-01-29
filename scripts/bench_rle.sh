#!/bin/bash

set -x ## Echo
set -e ## Exit if error

source scripts/utils.sh

# parameter space
rle_rec_limit_S="128 256 512"
xarr_rle_grain_S="16 32 64"
rles_S="500000 1000000 5000000"
sla_max_level_S="5 15 20"

repeats=4
resdir="/dev/shm"
xdate="$(date +%Y%m%d.%H%M%S)"

resdir="${resdir}/bench_rle.${xdate}"
mkdir $resdir

echo "Starting run at $(date) [output logs on ${resdir}]"

machine_info > $resdir/machine_info
git_info     > $resdir/git_info


function do_run() {
	xrepeats=$1
	xarr=$2
	makeargs=$3
	make ${makeargs} clean all >> $xresdir/build
	for rle_rec_limit in $rle_rec_limit_S; do
		for xarr_rle_grain in $xarr_rle_grain_S; do
			for rles in $rles_S; do
				for threads in 1 $(seq 2 2 $(nproc)); do
					for i in $(seq $xrepeats);  do
						CILK_NWORKERS=$threads         \
						RLE_REC_LIMIT=$rle_rec_limit   \
						XARR_RLE_GRAIN=$xarr_rle_grain \
						./rle/prle_rec_xarray_$xarr $rles
					done
				done
			done
		done
	done >> $xresdir/runlog
}

## Debug RUN
#xresdir="$resdir/debug"
#mkdir $xresdir
#make DEBUG_BUILD=1 clean all  > $xresdir/build
#do_run 1                      > $xresdir/runlog

## Results RUN
xresdir="$resdir/results"
mkdir $xresdir
for  sla_max_level in $sla_max_level_S; do
	do_run $repeats "sla" "NOSTATS_BUILD=1 SLA_MAX_LEVEL=$sla_max_level"
done
do_run $repeats "da" "NOSTATS_BUILD=1"

## Stats RUN
xresdir="$resdir/stats"
for  sla_max_level in $sla_max_level_S; do
	do_run $repeats "sla" "SLA_MAX_LEVEL=$sla_max_level"
done
do_run $repeats "da" ""

echo "Ending run at $(date) [output logs on ${resdir}]"
touch $resdir/COMPLETED
