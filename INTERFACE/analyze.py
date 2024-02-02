#!/usr/bin/env python3
# _*_ coding: ascii _*_

import os
import sys
import re
import math
from optparse import OptionParser
from mpmath import mp
from matplotlib import pyplot

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
    RATE = re.compile (r'^T: Rate: ' + _float)

class DSet:

    def __init__ (self):
        self.line_count = 0
        self.block_count = 0
        self.values = []
        self.samples = 0
        self.__nils = 0
        self.time = -1
        self.__end = 0
        self.rate = 0.0
        self.magnitude = []

    def add_line (self, line):
        self.line_count += 1
        m = Regs.BHDR.match (line)
        if m:
            try:
                b = int (m.group (1))
                t = int (m.group (2))
            except Exception:
                abt (f"line {self.line_count}, illegal line format: '{line}'")
            self.block_count += 1
            if self.block_count != b:
                abt (f"line {self.line_count}, block number {b} out of "
                    f"sequence, {self.block_count} expected")
            if self.time < 0:
                self.time = t
            self.__end = t;
            return

        m = Regs.VECTOR.match (line)
        if m:
            v = []
            for i in range (1, 4):
                b = float (m.group (i))
                v.append (b)
            if self.__nils:
                # we are filling in blanks
                if self.samples == 0:
                    # skip the --nils-- at the beginning
                    self.__nils = 0
                else:
                    f = self.values [-1].copy ()
                    while self.__nils:
                        d = [0.0, 0.0, 0.0]
                        for i in range (0, 3):
                            d [i] = (v [i] - f [i]) / (self.__nils + 1.0)
                            f [i] += d [i]
                        self.values.append (f.copy ())
                        self.__nils -= 1
            self.values.append (v)
            self.samples += 1
            return

        m = Regs.NIL.match (line)
        if m:
            # for now just mark it
            self.__nils += 1
            return

        m = Regs.RATE.match (line)
        if m:
            self.rate = float (m.group (1))
            return

    def close (self):
        if self.rate == 0.0:
            abt ("sampling rate undefined")
        if self.samples == 0:
            abt ("no samples found")
        self.time = self.samples / self.rate
        print (f'{self.samples} samples, {self.rate:7.3f} sps, '
            f'{self.time:4.3f} seconds')

    def cmag (self):
        self.magnitude = []
        for i in range (self.samples):
            self.magnitude.append (fmag (self.values [i]))

    def show_fun (self, fxform, xgroup, t0, t1, Y, np):
        # from t0 to t1, Y - max, np - number of points (grain)
        if t0 < 0.0 or t1 <= t0 or t1 > self.time:
            ant ("illegal time parameters for show_fun")
        ss = int ((t0 / self.time) * self.samples + 0.5)
        se = int ((t1 / self.time) * self.samples + 0.5)
        # extract and transform the sample points
        i = ss
        pts = []
        while i <= se:
            pts.append (fxform (self.values [i]))
            i += 1
        # group into np values
        pts = xgroup (pts, np)
        pyplot.plot (pts)
        pyplot.show ()

def read_data (ifn):

    global DataSet

    try:
        fd = open (ifn, "r", encoding="locale")
    except Exception as ex:
        abt (f'cannot open file {ifn} for reading, {ex}')

    DataSet = DSet ()

    for line in fd:
        DataSet.add_line (line)
    fd.close ()
    DataSet.close ()

def write_output (ofn):

    global DataSet

    try:
        fd = open (ofn, "w", encoding="locale")
    except Exception as ex:
        abt (f'cannot open file {ofn} for writing, {ex}')

    nl = len (DataSet.values)
    for i in range (0, nl):
        v = DataSet.values [i]
        m = ""
        for j in range (0, 3):
            m += f' {v[j]:7.4f}'
        fd.write (m + '\n');
    fd.close ()

def main ():
    global DataSet
    global POptions
    ps = OptionParser ("usage: %prog [options]")
    # later
    (options, args) = ps.parse_args ()
    # input file name
    if len (args) < 1:
        abt ("input file not specified")
    ifn = args [0]
    read_data (ifn)

    process_data (DataSet)

    if len (args) < 2:
        exit (0)

    ofn = args [1]
    write_output (ofn);

###############################################################################

def xf_mag (v):
    return math.sqrt (v [0] * v [0] + v [1] * v [1] + v [2] * v [2])

def xf_avg (pts):
    sz = len (pts)
    re = 0.0
    for i in range (sz):
        re += pts [i]
    return re / sz

def gr_avg (pts, np):
    # grouping values into np points; last one can be degenerate
    sz = len (pts)
    if sz <= np:
        # nothing to do
        return pts
    # group size
    gs = (sz + np - 1) // np
    re = []
    i = 0
    while i < sz:
        # lwa + 1
        j = i + gs
        if j > sz:
            j = sz
        re.append (xf_avg (pts [i:j]))
        i = j
    return re

def process_data (ds):

    ds.show_fun (xf_mag, gr_avg, 0.0, 30.0, 0, 1000)

###############################################################################
main ()
