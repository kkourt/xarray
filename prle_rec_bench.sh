#!/bin/bash

#export RLE_REC_LIMIT=1
#for i in $(seq 1 8)
#do
#echo ./prle_rec -nproc $i
#     ./prle_rec -nproc $i
#echo
#done

rles=1000000
rm -f Temp-C Temp-cilk Temp-results

# get simple C measurements
echo "*** Running serial (pure C) ***"
./rle_rec $rles | tee Temp-C | sed -e 's/^/  > /'

c_rle_enc=$(perl -n -e 'if (/^rle_encode:\s+(\S+) secs/){ print $1; }' < Temp-C)
c_rle_rec_enc=$(perl -n -e 'if (/^rle_encode_rec:\s+(\S+) secs/){ print $1; }' < Temp-C)

echo "C RLE encode:           $c_rle_enc"      | tee -a Temp-results
echo "C RLE recursive encode: $c_rle_rec_enc"  | tee -a Temp-results
echo


echo ./prle_rec $rles
     ./prle_rec $rles | tee Temp-cilk | sed -e 's/^/  > /'
cilk_rle_enc_serial=$(perl -n -e 'if (/^rle_encode:\s+(\S+) secs/){ print $1; }' < Temp-cilk)
cilk_rle_rec_enc_serial=$(perl -n -e 'if (/^rle_encode_rec:\s+(\S+) secs/){ print $1; }' < Temp-cilk)

echo "*** Running parallel (Cilk) ***"
for i in $(seq 1 8)
do
	echo ./prle_rec --nproc $i $rles
	     ./prle_rec --nproc $i $rles | tee Temp-cilk | sed -e 's/^/  > /'
	cilk_rle_rec_enc=$(perl -n -e 'if (/^rle_encode_rec:\s+(\S+) secs/){ print $1; }' < Temp-cilk)
	p1=$(printf "%2.2lf\n" $(echo "$c_rle_enc / $cilk_rle_rec_enc"  | bc -l))
	p2=$(printf "%2.2lf\n" $(echo "$c_rle_rec_enc / $cilk_rle_rec_enc"  | bc -l))
	p3=$(printf "%2.2lf\n" $(echo "$cilk_rle_enc_serial / $cilk_rle_rec_enc"  | bc -l))
	p4=$(printf "%2.2lf\n" $(echo "$cilk_rle_rec_enc_serial / $cilk_rle_rec_enc"  | bc -l))
	echo "Cilk RLE (thr:$i):       $cilk_rle_rec_enc (vs C: $p1) (vs C-rec: $p2) (vs cilk-serial:$p3) (vs cillk-serial-rec:$p4)"  | tee -a Temp-results
done
cilk_rle_limit=$(perl -n -e 'if (/^rle_rec_limit:\s*(\S+)/){ print $1; }' < Temp-cilk)

echo "*** Results ***"
echo "Number of RLES:         $rles"
echo "RLE recursion limit:    $cilk_rle_limit"
cat Temp-results
