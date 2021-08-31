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

	global qdrop fovfl mallf

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
				lappend res [list $bnum $bts $blk]
				set inb 0
			}

			continue
		}

		if [regexp "^(..:..:..).(...).*B: *(\[\[:digit:\]\]+)" \
		   $line mat tm ms bn] {
			# time stamp in milliseconds
			set bts "[clock scan $tm]$ms"
			# block number
			if [catch { expr { $bn } } bnum] {
				err "line $lnum, illegal block number $bn,\
					$bnum"
				continue
			}
			set inb 1
			set blk ""
			set blen 0
		
			continue
		}

		if [regexp {E: +.*F=(..)} $line mat flags] {
			set flags [expr { "0x$flags" }]
			if { [expr { $flags & 0x01 }] } {
				incr fovfl
			}
			if { [expr { $flags & 0x02 }] } {
				incr mallf
			}
			if { [expr { $flags & 0x04 }] } {
				incr qdrop
			}
		}
	}

	return $res
}

proc main { } {

	global argv qdrop fovfl mallf

	set fn [lindex $argv 0]
	if { $fn != "" } {
		if [catch { open $fn "r" } fd] {
			err "cannot open $fn, $fd"
		}
	} else {
		set fd "stdin"
	}

	set qdrop 0
	set fovfl 0
	set mallf 0

	set samples [scanfile $fd]

	set lbn 0
	set maxbn 0
	set minbn 999999999999999
	set nretr 0
	set ndupl 0
	set ngaps 0
	set nmiss 0

	foreach s $samples {
		set bn [lindex $s 0]
		if { $bn < $minbn } {
			set minbn $bn
			set mints [lindex $s 1]
		} 
		if { $bn > $maxbn } {
			set maxbn $bn
			set maxts [lindex $s 1]
		}
		if [info exists _a($bn)] {
			# puts "duplicate block: $bn"
			incr ndupl
		}
		set _a($bn) ""
		if { $bn <= $lbn } {
			# puts "retransmitted block: $bn ($lbn)"
			incr nretr
		} else {
			incr lbn
			if { $bn != $lbn } {
				set nm [expr { $bn - $lbn }]
				incr ngaps $nm
if 0 {
				if { $nm == 1 } {
					puts "missing block: $lbn"
				} else {
					puts "missing blocks: $lbn [expr { $bn -
						1 }]"
				}
}
				set lbn $bn
			}
		}
	}

	set lost ""
	for { set i $minbn } { $i <= $maxbn } { incr i } {
		if ![info exists _a($i)] {
			lappend lost $i
			incr nmiss
		}
	}

	# puts "$lbn blocks \[lost: [join $lost]\]"
	# calculate the effective rate
	set nb [expr { $maxbn - $minbn }]
	puts "Total blocks:   [expr { $nb + 1 }]"
	puts "Missing:        $nmiss"
	puts "Retransmitted:  $nretr"
	puts "Gaps:           $ngaps"
	puts "Failures:       $fovfl $mallf $qdrop"
	set tm [expr { $maxts - $mints }]
	set rt [expr { (($nb * 12) * 1000.0) / $tm }]
	puts "Rate:           [format %1.3f $rt] sps"
	puts "Missing blocks: [join $lost]"
}

main
