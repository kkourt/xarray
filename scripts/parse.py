#!/usr/bin/env python

from pprint import pprint

from pychart import *

from logparse import LogParser
from dict_hier import dhier_reduce, dhier_reduce_many
from kkstats import StatList
from xdict import xdict
from humanize_nr import humanize_nr

parse_conf = r'''
/^xarray impl: *(\S+)$/
	xarray = _g1

/^Number of threads: *(\d+).*$/
	nthreads = int(_g1)

/^ *rle_encode_rec: .*ticks \[ *(\d+)\]$/
	ticks = int(_g1)

/^number of rles: *(\d+)$/
	rles = int(_g1)

/^rle_rec_limit: *(\d+)$/
	rec_limit = int(_g1)

/^xarr_rle_grain: *(\d+)$/
	xarr_rle_grain = int(_g1)

/^DONE$/
	flush
'''

def do_parse(fname):
	f = open(fname)
	lp = LogParser(parse_conf, debug=False, globs={}, eof_flush=False)
	lp.go(f)
	d = dhier_reduce_many(
		lp.data,
		("rles", "rec_limit", "xarr_rle_grain", "xarray", "nthreads"),
		map_fn=lambda lod: StatList((x['ticks'] for x in lod))
	)
	return d


def do_plot(d, fname="plot.pdf"):

	theme.use_color = True
	theme.get_options()
	canv = canvas.init(fname=fname, format="pdf")

	ncores = max(d.itervalues().next().itervalues().next().iterkeys())

	ar_y = 0
	for rles, d0 in d.iteritems():
		for rec_limit, d1 in d0.iteritems():
			for xarr_rle_grain, d2 in d1.iteritems():
				ar  = area.T(
					x_axis  = axis.X(label="/10{}cores", format="%d"),
					y_axis  = axis.Y(label="/10{}speedup", format="%d"),
					y_range = [0,16],
					size = (400,200),
					loc  = (0, ar_y)
				)
				base = d2['DA'][1].avg
				for xarr, xarr_d in d2.iteritems():
					k = "%5s rec_limit:%d xarr_grain:%d rles:%.1f%s" \
						% ((xarr, rec_limit, xarr_rle_grain) + humanize_nr(rles))
					x = []
					for (n, n_d) in xarr_d.iteritems():
						n_d.setfn(lambda x : base / x)
						x.append((n, n_d.avg, n_d.avg_minus, n_d.avg_plus))
					x = sorted(x, key= lambda x: x[0])
					ar.add_plot(line_plot.T(
						data=x,
						label=k,
						y_error_minus_col=3,
						y_error_plus_col=3,
						error_bar = error_bar.error_bar2(tic_len=5, hline_style=line_style.gray50),
						tick_mark=tick_mark.X(size=3),
					))
				ar.draw()
				ar_y += 300
				line_plot.line_style_itr.reset()

	canv.close()

from sys import argv
if __name__ == '__main__':
	d = do_parse(argv[1])
	do_plot(d, fname=argv[1] + "-plot.pdf")
