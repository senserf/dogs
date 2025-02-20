#!/bin/sh
#
#	Copyright 2002-2020 (C) Olsonet Communications Corporation
#	Programmed by Pawel Gburzynski
#	All rights reserved
#
#	This file is part of the PICOS platform
#
########\
exec tclsh "$0" "$@"

# some stats to be collected along the way
set STA(QDROP)		0
set STA(FOVFL)		0
set STA(MALLF)		0
set STA(PDROP)		0
set STA(NOBSO)		0
set STA(NOORD)		0
set STA(NLOST)		0
set STA(LLOST)		""
set STA(NSTSH)		0
set STA(LTBLK)		0

# the smallest expected block numer
set SMEXP		1

set STASH ""
set STASH_MIN		0
set STASH_MAX		0

# current input line number
set ILNUM		0

proc err { m } {

	puts stderr $m
	exit 1
}

proc readline { } {

	global IFD ILNUM

	while 1 {
		if { [gets $IFD line] < 0 } {
			return ""
		}
		incr ILNUM
		if { $line != "" } {
			return $line
		}
		# ignore empty lines (there won't be any)
	}
}

proc to_f16 { w } {
#
# 16-bit to float
#
	if [expr { $w & 0x8000 }] {
		set w [expr { -65536 + $w }]
	}

	return [format %7.4f [expr { $w / 32768.0 }]]
}

proc wblk { bn bl { ts -1 } } {

	global OFD MARKS

	if { $ts < 0 } {
		# estimate block time
		set ts [ebt $bn]
	}

	while { $MARKS != "" } {
		# the time of the mark
		set tm [lindex $MARKS 0 0]
		if { $tm > $ts } {
			break
		}
		puts $OFD "@ [lindex $MARKS 0 1]"
		set MARKS [lrange $MARKS 1 end]
	}

	puts -nonewline $OFD "$bn $ts\n$bl"
}

proc ebt { bn } {
#
# Estimate block time based on current running rate
#
	variable TIMING

	if { $TIMING(b) == 0 } {
		# will not happen
		return 0
	}

	set t [expr { round($TIMING(A) +
		(($TIMING(B) - $TIMING(A)) / double($TIMING(b) - $TIMING(a))) *
			($bn - $TIMING(a))) } ]

	if { $t < 0 } {
		return 0
	} else {
		return $t
	}
}

proc wqstat { bb } {

	global CTS OFD LQO

	set qs [lindex $bb 0]
	set qo [lindex $bb 1]

	set qs [expr 0x$qs]
	set qo [expr 0x$qo]

	set qs [expr { ($qs >> 2) & 0x3fffffff }]
	set qo [expr { ($qo >> 2) & 0x3fffffff }]

	if { $qs > 63 || $qo != $LQO } {
		puts $OFD "QS: $CTS $qs $qo"
		set LQO $qo
	}
}

proc block_line { ln } {

	global ILNUM SMEXP STA CTS LIMIT MORE TIMING

	if ![regexp {([[:digit:]]+) (.*)} $ln ma bn vals] {
		err "illegal block line $ILNUM, $ln"
	}

	if { $bn < $SMEXP } {
		# the block number is less than smallest expected, just
		# ignore
		incr STA(NOBSO)
		return
	}

	# decode
	if { [llength $vals] != 12 } {
		err "illegal block line $ILNUM, $ln"
	}

	set bl ""

	foreach c $vals {

		if [catch { expr 0x$c } c] {
			err "illegal value in block in line ILNUM, $ln"
		}

		set x [to_f16 [expr { ($c >> 16) & 0xffc0 }]]
		set y [to_f16 [expr { ($c >>  6) & 0xffc0 }]]
		set z [to_f16 [expr { ($c <<  4) & 0xffc0 }]]

		append bl "$x $y $z\n"
	}

	wqstat $vals

	if { $bn == $SMEXP } {
		# on-time arrival
		wblk $bn $bl $CTS
		incr SMEXP
		if { $LIMIT && $bn >= $LIMIT } {
			set MORE 0
		} else {
			advance
		}
		if { $TIMING(a) == 0 } {
			set TIMING(a) $bn
			set TIMING(A) $CTS
			return
		}
		# calculate the running rate in milliseconds / block
		set TIMING(b) $bn
		set TIMING(B) $CTS
		return
	}

	# we still have a hole, stash the block
	stash $bn $bl
}

proc write_null { bn } {

	set bl ""

	for { set i 0 } { $i < 12 } { incr i } {
		append bl "--nil-- --nil-- --nil--\n"
	}

	wblk $bn $bl
}

proc add_lost { } {

	global SMEXP STA

	write_null $SMEXP

	incr STA(NLOST)

	if { [llength $STA(LLOST)] > 9 } {
		set STA(LLOST) [lrange $STA(LLOST) 1 end]
	}

	lappend STA(LLOST) $SMEXP
}
	
proc eot_line { ln } {

	global ILNUM STA SMEXP STASH STASH_MIN LIMIT MORE

	if { [scan $ln "%u %u %f %x" ls bk ba fg] != 4 } {
		err "illegal value in eot line $ILNUM, $ln"
	}

	# flags
	if { [expr { $fg & 0x01 }] } {
		incr STA(FOVFL)
	}
	if { [expr { $fg & 0x02 }] } {
		incr STA(MALLF)
	}
	if { [expr { $fg & 0x04 }] } {
		incr STA(QDROP)
	}

	if { [expr { $fg & 0xf0 }] } {
		incr STA(PDROP) [expr { ($fg >> 4) & 0x0f }]
	}

	# the oldest block that still can arrive after this point
	set ob [expr { $ls - $bk + 1 }]

	while { $MORE && $SMEXP < $ob } {
		# output a null block
		add_lost
		incr SMEXP
		if { $LIMIT && $SMEXP > $LIMIT } {
			set MORE 0
		} elseif { $STASH != "" && $STASH_MIN <= $SMEXP } {
			advance
		}
	}
}

proc stash { bn bl } {

	global STASH STASH_MIN STASH_MAX STA

	set it [list $bn $bl]

	if { $STASH == "" } {
		set STASH [list $it]
		set STASH_MAX $bn
		set STASH_MIN $bn
		incr STA(NOORD)
		return
	}

	if { $bn > $STASH_MAX } {
		lappend STASH $it
		incr STASH_MAX
		if { $bn > $STASH_MAX } {
			incr STA(NOORD)
			set STASH_MAX $bn
		}
		return
	}

	set ix 0
	foreach c $STASH {
		set cu [lindex $c 0]
		if { $cu < $bn } {
			incr ix
			continue
		}
		if { $cu == $bn } {
			incr STA(NOBSO)
			# ignore
			return
		}
		# insert before c, but check if not there already
		set STASH [linsert $STASH $ix $it]
		if { $ix == 0 } {
			set STASH_MIN $bn
		}
		incr STA(NOORD)
		return
	}
}

proc advance { } {
#
# Try to advance the stash
#
	global STASH STASH_MIN STASH_MAX SMEXP STA OFD LIMIT MORE

	if { $STASH == "" || $SMEXP < $STASH_MIN } {
		# no way
		return
	}

	while { $SMEXP > $STASH_MIN } {
		# cannot happen
		incr STA(NOBSO)
		set STASH [lrange $STASH 1 end]
		if { $STASH == "" } {
			return
		}
		set STASH_MIN [lindex $STASH 0 0]
	}

	while { $SMEXP == $STASH_MIN } {
		wblk $SMEXP [lindex $STASH 0 1]
		incr SMEXP
		if { $LIMIT && $SMEXP > $LIMIT } {
			set MORE 0
			return
		}
		set STASH [lrange $STASH 1 end]
		if { $STASH == "" } {
			return
		}
		set STASH_MIN [lindex $STASH 0 0]
	}
}

proc flush_stash { } {

	global STASH STA SMEXP LIMIT MORE

	# stash size at completion
	set STA(NSTSH) [llength $STASH]
	# last "good" block at termination
	set STA(LTBLK) [expr { $SMEXP - 1 }]

	while { $MORE && $STASH != "" } {
		lassign [lindex $STASH 0] bn bl
		while { $MORE && $SMEXP < $bn } {
			add_lost
			incr SMEXP
			if { $LIMIT && $SMEXP > $LIMIT } {
				set MORE 0
				return
			}
		}
		wblk $bn $bl
		set STASH [lrange $STASH 1 end]
		set SMEXP [expr { $bn + 1 }]
		if { $LIMIT && $SMEXP > $LIMIT } {
			set MORE 0
			return
		}
	}
}

proc out_put { t h v } {

	global OFD

	append h ":"
	set l [string length $h]
	if { $l < 20 } {
		append h [string repeat " " [expr { 20 - $l }]]
	}

	puts stderr "$h $v"
	puts $OFD "${t}: $h $v"
}

proc main { } {

	global argv IFD OFD ILNUM CTS STA SMEXP LIMIT MORE TIMING MARKS
	global LQO

	set LQO 0

	set fn [lindex $argv 0]

	if { $fn != "" } {
		if [catch { open $fn "r" } IFD] {
			err "cannot open $fn, $IFD"
		}
	} else {
		set IFD "stdin"
	}

	set fo [lindex $argv 1]

	if { $fo != "" } {
		if [catch { open $fo "w" } OFD] {
			err "cannot open $fo, $OFD"
		}
	} else {
		set OFD "stdout"
	}

	# read the header line
	set ln [readline]

	if { $ln == "" } {
		err "the input file is empty"
	}

	if { [scan $ln "%lu %u %u %u %u %u %u %u %u" \
	    tm op th lr rn ba ra co lm] != 9 } {
		err "bad header in the input file"
	}

	# output the header
	set ts [expr { $tm / 1000 }]
	set ms [expr { $tm % 1000 }]

	out_put "H" "Start time" \
		"[clock format $ts -format %c] <[format %03u $ms]>"
	out_put "H" "Options" [format %02x $op]
	out_put "H" "Range" $rn
	out_put "H" "Bandwidth" $ba
	out_put "H" "Rate" $ra

	set LIMIT $lm
	set MORE 1

	# for estimating block timing
	set TIMING(a) 0
	set TIMING(A) 0
	set TIMING(b) 0
	set TIMING(B) 0

	set MARKS ""

	while { $MORE } {
		set ln [readline]
		if { $ln == "" } {
			break
		}
		if ![regexp {^([[:digit:]]+) (.): (.*)} $ln ma CTS tp ln] {
			err "bad line $ILNUM: $ln"
		}
		if { $tp == "B" } {
			block_line $ln
		} elseif { $tp == "E" } {
			eot_line $ln
		} elseif { $tp == "M" } {
			if ![regexp {^[[:digit:]]+} $ln ma] {
				err "bad mark id, line number $ILNUM"
			}
			lappend MARKS [list $CTS $ma]
		} else {
			err "bad line type $tp, line number $ILNUM"
		}
	}

	# the tail
	flush_stash

	# estimate the rate

	if { $TIMING(b) == 0 } {
		set f 0.0
	} else {
		set f [expr { (double($TIMING(b) - $TIMING(a)) /
			($TIMING(B) - $TIMING(A))) * 12000.0 }]
		set f [format %1.3f $f]
	}

	set s [expr { $TIMING(B) / 1000 }]
	set h [expr { $s / 3600 }]
	set s [expr { $s - $h * 3600 }]
	set m [expr { $s / 60 }]
	set s [expr { $s - $m * 60 }]
 
	# statistics
	out_put "T" "Total blocks" [expr { $SMEXP - 1}]
	out_put "T" "Rate" $f
	out_put "T" "Accounted for" $STA(LTBLK)
	out_put "T" "Out of order"  $STA(NOORD)
	set w "$STA(NLOST) $STA(PDROP)"
	if $STA(NLOST) {
		append w " \[[join $STA(LLOST)]\]"
	}
	out_put "T" "Lost (OSS/Peg)" $w
	out_put "T" "Duplicate" $STA(NOBSO)
	out_put "T" "Queue drops" $STA(QDROP)
	out_put "T" "FIFO overflows" $STA(FOVFL)
	out_put "T" "Malloc faults"  $STA(MALLF)
	out_put "T" "Time" "$h hours, $m minutes, $s seconds"
}

main
