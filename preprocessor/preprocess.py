#!/usr/bin/env python3
import argparse
import time
from jinja2 import Environment, FileSystemLoader
from watchdog.events import FileSystemEventHandler, LoggingEventHandler
from watchdog.observers import Observer

class Zoom:
	def __init__(self,z, is_last):
		self.z = z
		self.is_last = is_last

	@property
	def elem(self):
		max_scale = 655360000 >> self.z
		min_scale = 655360000 >> (self.z+1)
		if self.is_last:
			return f"<MaxScaleDenominator>{max_scale}</MaxScaleDenominator>"
		else:
			return f"<MaxScaleDenominator>{max_scale}</MaxScaleDenominator><MinScaleDenominator>{min_scale}</MinScaleDenominator>"

def min_zoom_elem(min_zoom):
	scale = 655360000 >> min_zoom
	return f"<MaxScaleDenominator>{scale}</MaxScaleDenominator>"

def zooms(min_zoom,max_zoom):
	return [Zoom(z,z == max_zoom) for z in range(min_zoom,max_zoom+1)]

def interpolate_exp(exponent,*stops):
	def fn(zoom_obj):
		z = zoom_obj.z
		if z <= stops[0][0]:
			return stops[0][1]
		if z >= stops[-1][0]:
			return stops[-1][1]
		idx = 0
		while z < stops[idx][0]:
			idx = idx + 1
		normalized_x = (z - stops[idx][0]) / (stops[idx+1][0] - stops[idx][0])
		normalized_y = pow(normalized_x,exponent)
		return stops[idx][1] + normalized_y * (stops[idx+1][1] - stops[idx][1])
	return fn

def write(infile,outfile):
	env = Environment(
	    loader=FileSystemLoader('.'),
	    trim_blocks = True,
	    lstrip_blocks = True
	)
	try:	
		result = env.get_template(infile).render(
			enumerate=enumerate,
			min_zoom_elem=min_zoom_elem,
			zooms=zooms,
			interpolate_exp=interpolate_exp
		)
		with open(outfile,'w') as f:
			f.write(result)
	except Exception as e:
		print(e)

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Watch a Mapnik XML template.')
	parser.add_argument('input', type=str, help='Input jinja2 template')
	parser.add_argument('output', type=str, help='Output Mapnik XML')
	args = parser.parse_args()

	write(args.input,args.output)

	class Handler(FileSystemEventHandler):
		def on_any_event(self,event):
			write(args.input,args.output)

	observer = Observer()
	observer.schedule(Handler(),args.input)
	observer.start()
	try:
		observer.join()
	except KeyboardInterrupt:
		pass
