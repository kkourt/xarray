#!/usr/bin/env python

from pprint import pprint

from pychart import *

from logparse import LogParser, MultiFiles
from dict_hier import dhier_reduce, dhier_reduce_many, dhier_reduce_many2
from kkstats import StatList
from xdict import xdict
from humanize_nr import humanize_nr
from itertools import product
from copy import deepcopy
from math import floor, ceil
import os.path

xarr_parse_conf = r'''
/^xarray impl: *(\S+)$/
	xarray = _g1

/^Number of threads: *(\d+).*$/
	nthreads = int(_g1)

/^ *sum_rec_ALL: ticks: .*\[ *(\d+)\].*$/
	ticks = int(_g1)

/^number of ints: *(\d+)$/
	ints = int(_g1)

/^sum_rec_limit: *(\d+)$/
	rec_limit = int(_g1)

/^xarr_grain: *(\d+)$/
	xarr_grain = int(_g1)

/^SLA_MAX_LEVEL: *(\d+)$/
	sla_max_level = int(_g1)

/^DONE$/
	flush
'''

def do_parse(dirname):
	print dirname
	slamf = MultiFiles(iter([dirname + "/nostats-xarray_da-runlog",
	                        dirname + "/nostats-xarray_sla-runlog"]),
					 lambda x: "__START__: %s" % x,
					 lambda x: "__END__: %s" % x)
	lp = LogParser(xarr_parse_conf, debug=False, globs={}, eof_flush=False)

	lp.go(slamf)
	pprint(lp.data)
	sla_d, params = dhier_reduce_many2(
		deepcopy(lp.data),
		("ints", "rec_limit", "xarr_grain", "sla_max_level", "xarray",
		  "nthreads"),
		map_fn=lambda lod: StatList((x['ticks'] for x in lod))
	)
	return sla_d, params,

tick_marks = [
	tick_mark.Square(size=4, fill_style=fill_style.black),
	tick_mark.Circle(size=4),
	tick_mark.DownTriangle(size=4, fill_style=fill_style.black),
]
tick_mark_idx = 0

class MyLineP(line_plot.T):
    def get_legend_entry(self):
        ret = line_plot.T.get_legend_entry(self)
        ret.line_len = 20
        return ret;

def add_plot(myarea, label, base, mydata):
	global tick_mark_idx
	x = []
	for (n, n_d) in mydata.iteritems():
		n_d.setfn(lambda x : base / x)
		x.append((n, n_d.avg, n_d.avg_minus, n_d.avg_plus))
	x = sorted(x, key= lambda x: x[0])
	myarea.add_plot(MyLineP(
		data=x,
		label=label,
		y_error_minus_col=3,
		y_error_plus_col=3,
		error_bar = error_bar.error_bar2(tic_len=5, hline_style=line_style.gray50),
		tick_mark=tick_marks[tick_mark_idx % len(tick_marks)]
	))
	tick_mark_idx += 1

def do_plot_params(d, ncores, area_y, canv,
		           ints, rec_limit, xarr_grain, sla_max_level):
	global tick_mark_idx
	conf = "rec_limit:%d xarr_grain:%d sla_max_level:%d ints:%4.1f%s" \
	       % ((rec_limit, xarr_grain, sla_max_level) + humanize_nr(ints))
	print "*****", conf
	ar  = area.T(
		x_axis  = axis.X(label="/10{}cores", format="%d",
						 tic_interval = lambda xmin,xmax:
										[xmin] + range(xmin+1, xmax+1, 2)),
		y_axis  = axis.Y(label="/10{}speedup", format="%d", tic_interval=1),
		#y_grid_interval = 1,
		y_range = (0, None),
		y_grid_interval = lambda mn,mx: range(int(floor(mn)),1+int(ceil(mx))),
		x_range = (1, ncores),
		legend = legend.T(loc=(20,110)),
		size = (300,150),
		loc  = (0, area_y)
	)

	plot_d = d[ints][rec_limit][xarr_grain][sla_max_level]
	da_base = plot_d["DA"][1].avg

	add_plot(ar, "da ", da_base, plot_d["DA"])
	add_plot(ar, "sla", da_base, plot_d["SLA"])
	tick_mark_idx = 0
	ar.draw()

def do_plot_all(d, params, fname="plot.pdf"):
	theme.use_color = False
	theme.get_options()
	canv = canvas.init(fname=fname, format="pdf")
	ncores = max(params['nthreads'])
	print '-'*10, fname

	params_space = product(
		params["ints"],
		params["rec_limit"],
		params["xarr_grain"],
		params["sla_max_level"]
	)

	area_y = 0
	for (ints, rec_limit, xarr_grain, sla_max_level) in params_space:
		conf = "rec_limit:%d xarr_grain:%d sla_max_level:%d ints:%4.1f%s" \
			% ((rec_limit, xarr_grain, sla_max_level) + humanize_nr(ints))
		do_plot_params(d, ncores, area_y, canv,
		               ints, rec_limit, xarr_grain, sla_max_level)
		canv.show(100, area_y + 180, conf)
		area_y += 300
		line_plot.line_style_itr.reset()
	print "CLOSE"
	canv.close()

def do_plot(d, params, fname="plot.pdf"):
	theme.use_color = False
	theme.get_options()
	canv = canvas.init(fname=fname, format="pdf")
	ncores = max(params['nthreads'])
	do_plot_params(d, ncores, 0, canv, 5000000, 256, 32, 5)
	canv.close()

from sys import argv
if __name__ == '__main__':
	xarr_data, params = do_parse(argv[1])
	print xarr_data
	print params
	outfile, outext = os.path.splitext(argv[2])
	do_plot_all(deepcopy(xarr_data),
				params, fname=outfile + "-all" + outext)
	#do_plot(xarr_data, serial_data, rle_data, params, fname=argv[2])
