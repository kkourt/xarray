#!/bin/bash

xarr_grain_S="50 100 500 1000 5000 10000"
xints=100000000     # 10^7
sum_rec_limit=10000 # 10^4

## do da first
SUM_REC_LIMIT=$sum_rec_limit ./sum/sum_xarray_da $xints

## do multiple SLA runs, iterating over different xarr_grains
for xarr_grain in $xarr_grain_S; do
	SUM_REC_LIMIT=$sum_rec_limit XARR_GRAIN=$xarr_grain ./sum/sum_xarray_sla $xints
done
