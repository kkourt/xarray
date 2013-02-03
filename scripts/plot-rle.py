#!/usr/bin/env python

from pprint import pprint

from pychart import *

from logparse import LogParser
from dict_hier import dhier_reduce, dhier_reduce_many, dhier_reduce_many2
from kkstats import StatList
from xdict import xdict
from humanize_nr import humanize_nr
from itertools import product

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

/^SLA_MAX_LEVEL: *(\d+)$/
	sla_max_level = int(_g1)

/^DONE$/
	flush
'''

sla_max_level_def = 5 # glitch -- needed for DA parsing
myhier = ("rles", "rec_limit", "xarr_rle_grain", "xarray", "sla_max_level",
		  "nthreads")

def do_parse(fname):
	f = open(fname)
	lp = LogParser(parse_conf, debug=False, globs={}, eof_flush=False)
	lp.go(f)
	d,params = dhier_reduce_many2(
		lp.data,
		myhier,
		map_fn=lambda lod: StatList((x['ticks'] for x in lod))
	)
	pprint(params)
	return d,params

def add_plot(myarea, label, base, mydata):
	x = []
	for (n, n_d) in mydata.iteritems():
		n_d.setfn(lambda x : base / x)
		x.append((n, n_d.avg, n_d.avg_minus, n_d.avg_plus))
	x = sorted(x, key= lambda x: x[0])
	myarea.add_plot(line_plot.T(
		data=x,
		label=label,
		y_error_minus_col=3,
		y_error_plus_col=3,
		error_bar = error_bar.error_bar2(tic_len=5, hline_style=line_style.gray50),
		tick_mark=tick_mark.X(size=3),
	))

def do_plot2(d, params, fname="plot.pdf"):
	theme.use_color = True
	theme.get_options()
	canv = canvas.init(fname=fname, format="pdf")
	ncores = max(params['nthreads'])
	print '-'*10, fname

	params_space = product(
		params["rles"],
		params["rec_limit"],
		params["xarr_rle_grain"],
	)

	area_y = 0
	for rles, rec_limit, xarr_rle_grain in params_space:
		conf = "rec_limit:%d xarr_grain:%d rles:%4.1f%s" \
			% ((rec_limit, xarr_rle_grain) + humanize_nr(rles))
		ar  = area.T(
			x_axis  = axis.X(label="/10{}cores", format="%d"),
			y_axis  = axis.Y(label="/10{}speedup", format="%d"),
			y_range = [0,16],
			x_range = [1, ncores],
			size = (400,200),
			loc  = (0, area_y)
		)
		canv.show(100, area_y + 180, conf)
		plot_d = d[rles][rec_limit][xarr_rle_grain]
		da_data = plot_d['DA'][sla_max_level_def]
		da_base = da_data[1].avg
		add_plot(ar, "DA " + conf, da_base, da_data)

		sla_data = plot_d["SLA"]
		for sla_max_level, data0 in sla_data.iteritems():
			add_plot(ar, ("SLA (max_level:%3d)" % sla_max_level), da_base, data0)

		area_y += 300
		line_plot.line_style_itr.reset()
		ar.draw()
	print "CLOSE"
	canv.close()

from sys import argv
if __name__ == '__main__':
	data, params = do_parse(argv[1])
	do_plot2(data, params, fname=argv[1] + "-plot.pdf")
