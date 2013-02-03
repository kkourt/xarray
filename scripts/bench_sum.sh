#!/bin/bash

set -x ## Echo
set -e ## Exit if error

source scripts/utils.sh

# parameters
sum_rec_limit_S="128 256 512"
xarr_grain_S="512 1024 4096"
xnums=50000000

repeats=4
resdir="/dev/shm"
xdate="$(date +%Y%m%d.%H%M%S)"

resdir="${resdir}/bench_sum.${xdate}"
mkdir $resdir

echo "Starting run at $(date) [output logs on ${resdir}]"

machine_info > $resdir/machine_info
git_info     > $resdir/git_info

function do_runs() {
	xrepeats=$1
	makeargs=$2
	fprefix=$3
	make clean
	make -j $(nproc) ${makeargs} all > $resdir/$fprefix-build

	for xarr in "da" "sla"; do
		for xarr_grain in $xarr_grain_S; do
			for sum_rec_limit in $sum_rec_limit_S; do
				for threads in 1 $(seq 2 2 $(nproc)); do
					for i in $(seq $xrepeats);  do
						CILK_NWORKERS=$threads         \
						SUM_REC_LIMIT=$sum_rec_limit   \
						XARR_GRAIN=$xarr_grain         \
						./sum/psum_xarray_$xarr $xnums
					done
				done
			done
		done > $resdir/${fprefix}-xarray_${xarr}-runlog
	done
}


do_runs $repeats "NOSTATS_BUILD=1" "nostats"

#do_runs 1 "" "stats"
