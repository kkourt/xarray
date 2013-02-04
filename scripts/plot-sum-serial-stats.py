#!/usr/bin/env python

from pprint import pprint

from logparse import LogParser
from dict_hier import dhier_reduce_many
from kkstats import StatList
from kkpychart import *
from humanize_nr import humanize_nr
from xdict import xdict

#from pychart import *

parse_conf = r'''
/^xarray impl: *(\S+)$/
	xarray = _g1

/^number of ints: *(\d+).*$/
	size = int(_g1)

/^sum_rec_limit: *(\d+)$/
	rec_limit = int(_g1)

/^xarr_grain: *(\d+)$/
	xarr_grain = int(_g1)

/^ *sum_rec_ALL: ticks:.* \[ *(\d+)\].*$/
	ticks_all = int(_g1)

/^ *sum_seq: ticks:.* \[ *(\d+)\].*cnt.*$/
	ticks_seq = int(_g1)

/^ *sum_split: ticks:.* \[ *(\d+)\].*cnt.*$/
	ticks_split = int(_g1)

/^DONE$/
	flush
'''

tick_marks = [
	tick_mark.Square(size=4, fill_style=fill_style.black),
	tick_mark.Circle(size=4),
	tick_mark.DownTriangle(size=4, fill_style=fill_style.black),
]

class MyLineP(line_plot.T):
    def get_legend_entry(self):
        ret = line_plot.T.get_legend_entry(self)
        ret.line_len = 20
        return ret;

def do_parse(filename):
	lp = LogParser(parse_conf, debug=False, globs={}, eof_flush=False)
	f = open(filename)
	lp.go(f)
	d = dhier_reduce_many(
		lp.data,
		('size', 'rec_limit', 'xarray', 'xarr_grain'),
	)
	return d

def do_plot(data, fname="plot.pdf"):
	theme.use_color = False
	theme.get_options()
	chart_object.set_defaults(bar_plot.T, width = 17)
	canv = canvas.init(fname=fname, format="pdf")
	assert len(data) == 1
	size = data.iterkeys().next()
	data = data[size]
	assert len(data) == 1
	rec_limit = data.iterkeys().next()
	data = data[rec_limit]
	da_data = data['DA']
	assert(len(da_data) == 1)
	da_grain = da_data.iterkeys().next()
	da_data  = xdict(da_data[da_grain][0])

	plot_data = []

	var_ticks = da_data.ticks_all - da_data.ticks_seq - da_data.ticks_split
	assert(var_ticks > 0)
	plot_data.append((('DA'), da_data.ticks_seq, da_data.ticks_split, var_ticks))

	for sla_grain, sla_data in sorted(data['SLA'].iteritems()):
		coord = "SLA (%.0f%s)" % humanize_nr(sla_grain)
		d = xdict(sla_data[0])
		var_ticks = d.ticks_all - d.ticks_seq - d.ticks_split
		assert(var_ticks > 0)
		plot_data.append((coord, d.ticks_seq, d.ticks_split, var_ticks))

		p = (100.0*(float(d.ticks_all) - float(da_data.ticks_all))/float(da_data.ticks_all))
		cycles_split = float(d.ticks_split)/float(size)
		cycles_total = float(d.ticks_all - da_data.ticks_all)/float(size)
		print "Overhead of %-10s compared to DA: %.2f" % (coord, p)
		print "split overhead of %-10s in cycles per array element  %.2f" % (coord, cycles_split)
		print "total overhead of %-10s in cycles per array element  %.2f" % (coord, cycles_total)

	plot_data = map(
		lambda x: [x[0]] + list(map(lambda y: float(y)/float(size), x[1:])),
		plot_data)

	ar  = area.T(
		size    = (180, 150),
		legend = legend.T(loc=(130,120)),
		x_coord = category_coord.T(plot_data, 0),
		#y_coord = log_coord.T(),
		x_axis  = axis.X(label="", format="/6{}%s"),
		y_axis  = axis.Y(label="/8cycles per array element", format="/8{}%.1f"),
		y_grid_interval = .5,
		y_range = [2,None],
	)

	plot1 = bar_plot.T(label="/8seq",   hcol=1, data=plot_data)
	plot2 = bar_plot.T(label="/8split", hcol=2, data=plot_data, stack_on = plot1)
	plot3 = bar_plot.T(label="/8other", hcol=3, data=plot_data, stack_on = plot2)
	ar.add_plot(plot1, plot2, plot3)
	ar.draw()
	canv.close()

if __name__ == '__main__':
	from sys import argv
	data = do_parse(argv[1])
	do_plot(data, argv[2])
