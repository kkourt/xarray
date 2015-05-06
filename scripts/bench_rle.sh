#!/bin/bash

set -x ## Echo
set -e ## Exit if error

source scripts/utils.sh

# parameters
rle_rec_limit_S="512 1024"
xarr_rle_grain_S="32 64"
xarr_impl_S="da sla rpa"
rles=5000000

repeats=4
resdir="/dev/shm"
xdate="$(date +%Y%m%d.%H%M%S)"

resdir="${resdir}/bench_rle.${xdate}"
mkdir $resdir

echo "Starting run at $(date) [output logs on ${resdir}]"

machine_info > $resdir/machine_info
git_info     > $resdir/git_info
echo $0 $*   > $resdir/cmd

function do_runs() {
	xrepeats=$1
	makeargs=$2
	fprefix=$3
	make clean
	make -j $(nproc) ${makeargs} all > $resdir/$fprefix-build

	for xarr in $xarr_impl_S; do
		for xarr_rle_grain in $xarr_rle_grain_S; do
			for rle_rec_limit in $rle_rec_limit_S; do
				for threads in 1 $(seq 2 2 $(nproc)); do
					for i in $(seq $xrepeats);  do
						CILK_NWORKERS=$threads         \
						RLE_REC_LIMIT=$rle_rec_limit   \
						XARR_RLE_GRAIN=$xarr_rle_grain \
						./rle/prle_rec_xarray_$xarr $rles
					done
				done
			done
		done > $resdir/${fprefix}-xarray_${xarr}-runlog
	done

	for rle_rec_limit in $rle_rec_limit_S; do
		for threads in 1 $(seq 2 2 $(nproc)); do
			for i in $(seq $xrepeats);  do
				CILK_NWORKERS=$threads         \
				RLE_REC_LIMIT=$rle_rec_limit   \
				./rle/prle_rec $rles
			done
		done
	done > $resdir/${fprefix}-noxarray-runlog
}

do_runs $repeats "NOSTATS_BUILD=1" "nostats"
do_runs 1 "" "stats"
