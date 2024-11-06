#!/usr/bin/env python3
# _*_ coding: ascii _*_

import os
import sys
import re
import math
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import argparse

###############################################################################

def abt (msg):
	print (msg, file=sys.stderr);
	exit (1);

class Regs:
	# precompile regular expressions
	BHDR = re.compile (r'^([0-9]+) +([0-9]+) *$')
	_float = r' *(-?[0-9]+\.[0-9]+)'
	VECTOR = re.compile (_float + ' ' + _float + ' ' + _float)
	NIL = re.compile (r'^--nil--')
	MARK = re.compile (r'^@ ([0-9]+)')
	DHDR = re.compile (r'^H: (.*)')
	DTRR = re.compile (r'^T: (.*)')
	RATE = re.compile (r'Rate: ' + _float)

class DSet:

# This represents the data set which in principle is going to be flexible
# allowing us for various operations. We start small and clumsy with the
# intention of updating this as we go.

	# samples per block
	SPB = 12

	def __init__ (self, ifn, toff = 0.0):

		# create the set from the input file representing a "cooked" sequence
		# of samples produced by the collector

		self.number_of_samples = 0
		self.number_of_marks = 0
		# assume this is specified in fractional seconds; we convert to
		# milliseconds
		self.time_offset = toff
		self.duration = -1
		self.line_count = 0
		self.block_count = 0
		self.filename = ifn
		self.values = []
		self.rate = 0.0
		self.marks = []
		self.complete = False
		self.missing = 0
		self.__nils = 0
		# uninitialized
		self.__stime = -1
		self.__ctime = 0
		self.__lmark = -1
		self.__rsc = DSet.SPB

		try:
			self.__fd = open (ifn, "r")
		except Exception as ex:
			raise Exception (f'cannot open file {ifn} for reading, {ex}')

		# header present
		hp = False
		# trailer present
		tp = False
		while 1:
			line = self.read_next_line ()
			if line == None:
				self.fmterr ('premature EOF')
			m = Regs.DHDR.match (line)
			if not m:
				# first non-header line
				if not hp:
					fmterr ('header not found')
				break
			# for now, we ignore the header except for checking for its formal
			# presence; put in here code for processing the individual lines
			# of the header
			hp = True

		# now for the samples; first line already in
		while 1:
			# look for trailer
			m = Regs.DTRR.match (line)
			if m:
				break
			self.add_line (line)
			# next line
			line = self.read_next_line ()
			if line == None:
				# trailer expected
				self.fmterr ('trailer missing')

		# the trailer
		while 1:
			# the trailer proper
			line = m.group (1)
			m = Regs.RATE.match (line)
			if m:
				self.rate = float (m.group (1))
				tp = True
			# other components of the trailer
			# ...
			line = self.read_next_line ()
			if line == None:
				# all done
				break
			# can only be trailer
			m = Regs.DTRR.match (line)
			if not m:
				self.fmterr ('garbage in trailer')

		# complete
		self.close ()

	def fmterr (self, msg = ""):
		m = f'illegal contents of the input file {self.filename}'
		if self.line_count > 0:
			m += f', line {self.line_count}'
		if msg:
			m += ', ' + msg
		raise Exception (m)

	def read_next_line (self):
		try:
			line = self.__fd.readline ()
		except Exception as e:
			raise Exception (r'cannot read from {self.filename}, ' + str (e))
		if not line:
			# close the file automatically
			self.__fd.close ()
			del self.__fd
			return None
		# strip the newline at the end
		self.line_count += 1
		return line [0:len(line)-1]

	def add_line (self, line):

		m = Regs.BHDR.match (line)
		if m:
			# block header
			if self.__rsc != DSet.SPB:
				# running sample count must be complete
				self.fmterr ('illegal block size')
			try:
				b = int (m.group (1))
				t = int (m.group (2))
			except Exception:
				self.fmterr ('illegal block header')
			self.__rsc = 0
			if self.block_count == 0:
				# this is the first block, save the first time stamp
				self.__stime = t
			# current time stamp
			self.__ctime = t
			self.block_count += 1
			if self.block_count != b:
				self.fmterr ('illegal block number')
			return

		m = Regs.VECTOR.match (line)
		if m:
			if self.__rsc == DSet.SPB:
				# we are at the limit
				self.fmterr ('block size exceeded')
			# construct the vector
			v = []
			for i in range (1, 4):
				b = float (m.group (i))
				v.append (b)
			# check if should fill the nils
			if self.__nils:
				# yes, this can only happen at a block boundary
				if self.__rsc != 0:
					self.fmterr ('nil block not on block boundary')
				if self.number_of_samples == 0:
					# in case this happens at the beginning
					f = [0.0, 0.0, 0.0]
				else:
					# previous vector
					f = self.values [-1].copy ()
				# __nils is a multiple of block size
				while self.__nils:
					# initialize
					d = [0.0, 0.0, 0.0]
					for i in range (0, 3):
						# interpolation
						d [i] = (v [i] - f [i]) / (self.__nils + 1.0)
						f [i] += d [i]
					self.values.append (f.copy ())
					self.__nils -= 1
					self.number_of_samples += 1
			self.values.append (v)
			self.__rsc += 1
			self.number_of_samples += 1
			return

		m = Regs.NIL.match (line)
		if m:
			# a missing block
			if self.__nils % DSet.SPB != self.__rsc:
				# must start at a block boundary
				self.fmterr ('nil value half way through the block')
			# count them
			self.__nils += 1
			self.__rsc += 1
			self.missing += 1
			return

		m = Regs.MARK.match (line)
		if m:
			mark = m.group (1)
			try:
				mark = int (mark)
			except Exception:
				self.fmterr ('illegal mark code')
			if mark != self.__lmark:
				self.__lmark = mark
				self.number_of_marks += 1
				self.marks.append ((self.__ctime, mark))
			return

	def close (self):
		if self.rate == 0.0:
			# impossible
			self.fmterr ('bad header, sampling rate undefined')
		if self.number_of_samples == 0:
			self.fmterr ('no samples found')
		# sanity check
		if self.number_of_samples != len (self.values):
			self.fmterr ('internal error, bad size of values')
		# compute this from ctime correcting for the last block; duration is
		# expressed in seconds
		self.duration = (self.__ctime - self.__stime) / 1000.0 + \
			(DSet.SPB / self.rate)

		nm = len (self.marks)
		for i in range (0, nm):
			t, m = self.marks [i]
			t = (t - self.__stime) / 1000.0
			if t < 0.0:
				t = 0.0
			self.marks [i] = (t, m)

		print (
			f'Number of samples:  {self.number_of_samples:8d}\n'
			f'Missing samples:    {self.missing:8d}\n'
			f'Number of marks:    {nm:8d}\n'
			f'Samples per second: {self.rate:8.3f}\n'
			f'Duration:           {self.duration:8.3f} seconds\n'
			f'Time offset:        {self.time_offset:8.3f} seconds\n',
			end = ""
		)

		self.complete = True
		# delete temporary variables
		del self.__nils
		del self.__stime
		del self.__ctime
		del self.__rsc

	def graph (self, fxform, t0 = -1.0, t1 = -1.0):
		# from t0 to t1, np - number of points (grain)
		if t0 < 0.0:
			a = 0.0
		else:
			a = t0 - self.time_offset
		if t1 < 0.0:
			b = self.duration
		else:
			b = t1 - self.time_offset
		em = None
		if a < 0.0:
			em = "start time out of range"
		elif b > self.duration:
			em = "end time out of range"
		elif a >= b:
			em = "start time >= end time"
		if em:
			raise Exception ("graph: " + em)
		# create point pairs
		ss = int ((a / self.duration) * self.number_of_samples + 0.5)
		se = int ((b / self.duration) * self.number_of_samples + 0.5) + 1
		if se > self.number_of_samples:
			se = self.number_of_samples

		# extract and time the points, calculate total min and max, identify
		# markers, create point sets
		ymin = None
		ymax = None
		# a pset is a tuple: mark, X, Y
		psets = []
		Time = []
		Vals = []
		# current mark = none
		cmark = -1
		# index of the next mark in marks
		imark = 0
		# the number of entries in marks
		nmarks = len (self.marks)
		# determine the first limit
		if nmarks:
			# we do have marks at all
			cmtime = self.marks [0][0]
		else:
			# no marks, use a sure sentinel
			cmtime = self.duration + 1.0

		# transform the points as required
		points = fxform (self.values)

		for i in range (0, self.number_of_samples):
			# convert to values as requested
			y = points [i]
			# calculate total min and max
			if ymin == None or y < ymin:
				ymin = y
			if ymax == None or y > ymax:
				ymax = y
			# relative time
			t = (i * self.duration) / self.number_of_samples
			if t >= cmtime:
				if Vals:
					# we do have a pending collection of points
					psets.append ((cmark, Time.copy (), Vals.copy ()))
					# reset
					Time = []
					Vals = []
				# switch the mark
				cmark = self.marks [imark][1]
				# look for next mark
				imark += 1
				if imark < nmarks:
					cmtime = self.marks [imark] [0]
				else:
					cmtime = self.duration + 1.0
			if i >= ss and i < se:
				# the point falls into our graph
				Time.append (t + self.time_offset)
				Vals.append (y)
		# done with the loop, check if have a pending set
		if Vals:
			# can use the originals now, won't be overwritten anymore
			psets.append ((cmark, Time, Vals))
		fig, ax = plt.subplots (figsize=(10.0, 3.0), layout='constrained')
		ax.axis ([a + self.time_offset, b + self.time_offset, ymin, ymax])
		for i in range (0, len (psets)):
			mark, X, Y = psets [i]
			color = mcolor (mark)
			ax.plot (X, Y, color = color)
		plt.show ()

def mcolor (mark):
#
# returns the color corresponding to the mark
#
	if mark < 1:
		return 'black'
	if mark < 2:
		return 'red'
	if mark < 3:
		return 'blue'
	if mark < 4:
		return 'orange'
	if mark < 5:
		return 'green'
	return 'orange'

def write_output (ofn):

	global ds

	try:
		fd = open (ofn, "w")
	except Exception as ex:
		abt (f'cannot open file {ofn} for writing, {ex}')

	nl = len (ds.values)
	for i in range (0, nl):
		v = ds.values [i]
		m = ""
		for j in range (0, 3):
			m += f' {v[j]:7.4f}'
		fd.write (m + '\n');
	fd.close ()

def write_marks (mfn):

	global ds

	try:
		fd = open (mfn, "w")
	except Exception as ex:
		abt (f'cannot open file {mfn} for writing, {ex}')

	nl = len (ds.marks)
	for i in range (0, nl):
		v = ds.marks [i]
		fd.write (f'{v [0]:8.3f} {v [1]:3d}\n')
	fd.close ()

def call_options ():

	ps = argparse.ArgumentParser (
		prog='analyze',
		description='analyzes cooked output',
		epilog='')

	ps.add_argument ('files', nargs='*', help='files: input, output, marks')
	ps.add_argument ("-t", "--time-offset", type=float, dest='toffset',
		help='time offset sssss.mmm')
	ps.add_argument ("-c", "--command-mode", action='store_true',
		dest='cmnd', help='command mode, overrides other arguments')

	return ps.parse_args ()
	
###############################################################################

def init_globals ():

	global ds
	global alpha
	global width

	ds = None
	alpha = 0.5
	width = 1

###############################################################################

def mag (v):
	return math.sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2])

def avg (p):
	a = 0.0
	n = 0
	for i in range (0, len (p)):
		a += p [i]
		n += 1
	if not n:
		return 0.0
	return a / n
	
###############################################################################

def gmag (vals):
	p = []
	for i in range (0, len (vals)):
		p.append (mag (vals [i]))
	return p

def gdev (vals):
	p = gmag (vals)
	a = avg (p)
	for i in range (0, len (p)):
		p [i] = p [i] - a
	return p

def gabs (vals):
	p = gdev (vals)
	for i in range (0, len (p)):
		if p [i] < 0.0:
			p [i] = -p[i]
	return p
		
def gema (vals):
	global alpha
	p = gabs (vals)
	a = avg (p)
	for i in range (0, len (p)):
		a = p [i] * alpha + a * (1 - alpha)
		p [i] = a
	return p

def gave (vals):
	global width
	p = gabs (vals)
	q = []
	if width < 2:
		return p
	for i in range (0, len (p)):
		j = i - width
		if j < 0:
			j = 0
		n = 0
		s = 0.0
		while j <= i:
			s = s + p [j]
			n += 1
			j += 1
		q.append (s / n)
	return q

###############################################################################

def process_data (ds):

	ds.graph (gmag)

###############################################################################

def do_command_mode ():

	print ("command mode")

	while 1:
		sys.stdout.write ("=> ")
		sys.stdout.flush ()
		line = sys.stdin.readline ().strip ()
		if line == 'q' or line == 'quit':
			break
		try:
			exec (line, globals ())
		except Exception as e:
			print ("failed: " + str (e))

	print ("exit")
		
def main ():

	global ds

	init_globals ()
	opts = call_options ()
	cm = opts.cmnd
	ds = None
	if cm or len (opts.files) == 0:
		# this overrides everything else
		do_command_mode ()
		return
	ifn = opts.files [0]
	ofn = None
	mfn = None
	# this can be None
	if len (opts.files) > 1:
		ofn = opts.files [1]
	# this too
	if len (opts.files) > 2:
		mfn = opts.files [2]
	to = opts.toffset
	if to == None:
		to = 0.0
	ds = DSet (ifn, to)
	process_data (ds)
	if ofn:
		write_output (ofn)
	if mfn:
		write_marks (mfn)

main ()


