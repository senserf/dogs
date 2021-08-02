#!/bin/sh
#
#	Copyright 2002-2020 (C) Olsonet Communications Corporation
#	Programmed by Pawel Gburzynski & Wlodek Olesinski
#	All rights reserved
#
#	This file is part of the PICOS platform
#
########\
exec tclsh "$0" "$@"

proc err { m } {

	puts stderr $m
}

proc scanfile { fd } {

	set res ""
	set lnum 0
	set inb 0

	while { [gets $fd line] >= 0 } {

		incr lnum

		if { $inb } {

			if ![regexp "^\[-\\. 0-9\]+$" $line] {
				err "line $lnum, block $bnum, \
					illegal line: $line"
				set inb 0
				continue
			}

			lappend blk $line
			incr blen

			if { $blen == 12 } {
				lappend res [list $bnum $tim $blk]
				set inb 0
			}

			continue
		}

		if [regexp "(..:..:..)\\.(...).*B: *(\[\[:digit:\]\]+)" \
			$line mat tim ms bn] {

			if [catch { expr { $bn } } bnum] {
				err "line $lnum, illegal block number $bn,\
					$bnum"
				continue
			}
			if [catch { clock scan $tim } sec] {
				err "line $lnum, block $bnum, illegal time\
					$tim, $sec"
				continue
			}
			regsub "^0+"  $ms "" ms
			if { $ms == "" } { set ms 0 }
			if { [catch { expr { $ms } } mse] || $mse > 999 } {
				err "line $lnum, block $bnum, illegal millisec\
					count $ms, $mse"
				continue
			}
			set tim [expr { $sec * 1000 + $mse }]
			set inb 1
			set blk ""
			set blen 0
		}
	}

	return $res
}

proc main { } {

	set samples [scanfile "stdin"]

	set lbn 0
	foreach s $samples {
		set bn [lindex $s 0]
		if [info exists _a($bn)] {
			puts "duplicate block: $bn"
		}
		set _a($bn) ""
		if { $bn <= $lbn } {
			puts "retransmitted block: $bn ($lbn)"
		} else {
			incr lbn
			if { $bn != $lbn } {
				set nm [expr { $bn - $lbn }]
				if { $nm == 1 } {
					puts "missing block: $lbn"
				} else {
					puts "missing blocks: $lbn [expr { $bn -
						1 }]"
				}
				set lbn $bn
			}
		}
	}

	set lost ""
	for { set i 1 } { $i <= $lbn } { incr i } {
		if ![info exists _a($i)] {
			lappend lost $i
		}
	}

	puts "$lbn blocks \[lost: [join $lost]\]"
}

main
