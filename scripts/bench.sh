#!/bin/bash

repeats=4

#set -x

for xarr in "da" "sla"
do
	for rles in 100000 10000000 #500000 1000000 5000000 10000000
	do
		for threads in $(seq $(nproc))
		do
			for i in $(seq $repeats)
			do
				echo CILK_NWORKERS=$threads ./prle_rec_xarray_$xarr $rles
				CILK_NWORKERS=$threads ./prle_rec_xarray_$xarr $rles
			done
		done
	done
done

