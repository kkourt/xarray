#!/bin/bash

set -x ## Echo
set -e ## Exit if error

source scripts/utils.sh

# parameter space
rle_rec_limit_S="128 256 512"
xarr_rle_grain_S="16 32 64"
rles_S="100000 1000000 10000000"

repeats=3
resdir="/dev/shm"
xdate="$(date +%Y%m%d.%H%M%S)"

resdir="${resdir}/bench_rle.${xdate}"
mkdir $resdir

echo "Starting run at $(date) [output logs on ${resdir}]"

machine_info > $resdir/machine_info
git_info     > $resdir/git_info


function do_run() {
	xrepeats=$1
	for xarr in "da" "sla"; do
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
		done
	done
}

## Debug RUN
#xresdir="$resdir/debug"
#mkdir $xresdir
#make DEBUG_BUILD=1 clean all  > $xresdir/build
#do_run 1                      > $xresdir/runlog

## Results RUN
xresdir="$resdir/results"
mkdir $xresdir
make NOSTATS_BUILD=1 clean all > $xresdir/build
do_run $repeats                > $xresdir/runlog

## Stats RUN
xresdir="$resdir/stats"
mkdir $xresdir
make clean all                 > $xresdir/build
do_run 1                       > $xresdir/runlog

echo "Ending run at $(date) [output logs on ${resdir}]"
touch $resdir/COMPLETED
