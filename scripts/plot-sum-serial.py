#!/usr/bin/env python

from pprint import pprint

from logparse import LogParser
from dict_hier import dhier_reduce_many
from kkstats import StatList
#from xdict import xdict

from pychart import *

#size:100000000        grain:5000             DA_ticks:184758168        SLA_ticks:195272803

parse_conf = r'''

/^size:(\d+) *grain:(\d+) *DA_ticks:(\d+) *SLA_ticks:(\d+).*$/
	size      = int(_g1)
	grain     = int(_g2)
	da_ticks  = int(_g3)
	sla_ticks = int(_g4)
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
		('size', 'grain'),
		map_fn=lambda lod: {'sla_ticks': StatList((x['sla_ticks'] for x in lod)),
		                    'da_ticks' : StatList((x['da_ticks'] for x in lod))}
	)
	return d

def do_plot(data, fname="plot.pdf"):
	theme.use_color = False
	theme.get_options()
	canv = canvas.init(fname=fname, format="pdf")
	assert len(data) == 1
	size = data.iterkeys().next()
	data = data[size]
	#pprint(data)

	ar  = area.T(
		x_coord = log_coord.T(),
		x_axis  = axis.X(label="/10{}/Hchunk size (/Tints/H)", format="%d",
						 tic_interval = lambda xmin,xmax: [500,1000,2000,5000,10000,20000,50000]),
		y_axis  = axis.Y(label="/10{}/Hadditions per tick", format="%.2f"),
		y_grid_interval = .02,
		#y_range = (0, None),
		#y_grid_interval = lambda mn,mx: range(int(floor(mn)),1+int(ceil(mx))),
		#x_range = (1, ncores),
		#legend = legend.T(loc=(20,110)),
		size = (300,150),
		#loc  = (0, area_y)
		x_range = (500, 50000),
		y_range = (.16,.24)
	)

	plot_data = {'da_ticks':[], 'sla_ticks':[]}
	for (grain, grain_d) in data.iteritems():
		#print grain, da.avg, sla.avg
		for k,l in grain_d.iteritems():
			l.setfn(lambda x : float(size) / float(x))
			plot_data[k].append((grain, l.avg, l.avg_minus, l.avg_plus))

	#pprint(plot_data)
	idx = 0
	for label, x in plot_data.iteritems():
		x = sorted(x, key= lambda x: x[0])
		print x
		ar.add_plot(MyLineP(
			data=x,
			label=label[:-6],
			y_error_minus_col=3,
			y_error_plus_col=3,
			error_bar = error_bar.error_bar2(tic_len=5, hline_style=line_style.gray50),
			tick_mark=tick_marks[idx]
		))
		idx += 1
	ar.draw()
	canv.close()

if __name__ == '__main__':
	from sys import argv
	data = do_parse(argv[1])
	do_plot(data)
