
plots = results/dethf-sum.pdf \
	results/sbrinz-rle.pdf
	#results/dethf-rle.pdf results/sbrinz-rle.pdf results/gottardo-rle.pdf

all: $(plots)

results/dethf-sum.pdf: results/bench_sum.dethf.20130203.215223/nostats-xarray_sla-runlog
	./scripts/plot-sum.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@

results/sbrinz-sum.pdf: results/bench_sum.sbrinz.20130203.204410/nostats-xarray_sla-runlog
	./scripts/plot-sum.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@

results/sbrinz-rle.pdf: results/bench_rle.sbrinz.20130203.194712/nostats-xarray_sla-runlog
	./scripts/plot-rle-paper.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@


results/gottardo-rle.pdf: results/bench_rle.gottardo.20130204.202626/nostats-xarray_sla-runlog
	./scripts/plot-rle-paper.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@



results/sbrinz-sumsqrt.pdf: results/bench_sumsqrt.sbrinz.20130203.220131/nostats-xarray_sla-runlog
	./scripts/plot-sum.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@



### FIXME

results/dethf-rle.pdf: results/bench_rle_paper.dethf.20130202.200709/nostats-xarray_sla-runlog
	./scripts/plot-rle-paper.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@

#results/gottardo-rle.pdf: results/bench_rle_paper.gottardo.20130202.231736/nostats-xarray_sla-runlog
#	./scripts/plot-rle-paper.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
#	cp  $(shell dirname $^)/$(shell basename $@) $@

results/appenzeller-rle.pdf: results/bench_rle_paper.appenzeller/nostats-xarray_sla-runlog
	./scripts/plot-rle-paper.py $(shell dirname $^) $(shell dirname $^)/$(shell basename $@)
	cp  $(shell dirname $^)/$(shell basename $@) $@

clean:
	rm -f $(plots)
