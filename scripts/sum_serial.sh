xarr_grain_S="500 1000 2000 5000 10000 20000 50000"
xints=100000000
repeats=4

for xarr_grain in $xarr_grain_S; do
	for i in $(seq $repeats); do
		cmd_da="SUM_REC_LIMIT=$xarr_grain XARR_GRAIN=$xarr_grain ./sum/sum_xarray_da  $xints"
		cmd_sla="SUM_REC_LIMIT=$xarr_grain XARR_GRAIN=$xarr_grain ./sum/sum_xarray_sla $xints"
		ticks_da=$(eval   $cmd_da | grep sum_rec_ALL | sed -e 's/^.*\[ *\(.*\)\].*$/\1/')
		ticks_sla=$(eval $cmd_sla | grep sum_rec_ALL | sed -e 's/^.*\[ *\(.*\)\].*$/\1/')
		printf "size:%-16s grain:%-16s DA_ticks:%-16s SLA_ticks:%-16s\n" $xints $xarr_grain $ticks_da $ticks_sla
	done
done
