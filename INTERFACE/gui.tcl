#!/bin/sh
#
#	Copyright 2002-2020 (C) Olsonet Communications Corporation
#	Programmed by Pawel Gburzynski & Wlodek Olesinski
#	All rights reserved
#
#	This file is part of the PICOS platform
#
#
########\
exec tclsh "$0" "$@"

package require Tk
package require Ttk

###############################################################################
# Determine the system type ###################################################
###############################################################################
if [catch { exec uname } ST(SYS)] {
	set ST(SYS) "W"
} elseif [regexp -nocase "linux" $ST(SYS)] {
	set ST(SYS) "L"
} elseif [regexp -nocase "cygwin" $ST(SYS)] {
	set ST(SYS) "C"
} else {
	set ST(SYS) "W"
}

# no DOS path type
set ST(DP) 0

if { $ST(SYS) != "L" } {
	# sanitize arguments; here you have a sample of the magnitude of
	# stupidity I have to fight when glueing together Windows and Cygwin
	# stuff; the last argument (sometimes!) has a CR character appended
	# at the end, and you wouldn't believe how much havoc that can cause
	set u [string trimright [lindex $argv end]]
	if { $u == "" } {
		set argv [lreplace $argv end end]
	} else {
		set argv [lreplace $argv end end $u]
	}

	# Not Linux: issue a dummy reference to a file path to eliminate the
	# DOS-path warning triggered at the first reference after Cygwin
	# startup

	set u [file normalize [pwd]]
	catch { exec ls $u }

	if [regexp -nocase "^\[a-z\]:" $u] {
		# DOS paths
		set ST(DP) 1
	}
	unset u
}

###############################################################################
# Determine the way devices are named; if running natively under Cygwin, use
# Linux style
###############################################################################

if [file isdirectory "/dev"] {
	set ST(DEV) "L"
} else {
	set ST(DEV) "W"
}

###############################################################################

set ST(VER)	0.1
set ST(HSK)	0
set ST(SFD)	""
set ST(OPREF)	1
set ST(ACT)	0
set ST(LAS)	0
set ST(SCB)	""
set ST(REX)	0
set ST(SDS)	""

set PM(TRL)	1000
set PM(EMU)	0
set PM(USP)	115200
set PM(MPL)	56
set PM(PXI)	65570
set PM(NID)	1
set PM(RET)	4000

set FFont {-family courier -size 10}
set SFont {-family courier -size 9}

###############################################################################

package provide unames 1.0
##########################################################################
# This is a package for handling the various names under which COM ports #
# may appear in our messy setup.                                         #
# Copyright (C) 2012 Olsonet Communications Corporation.                 #
##########################################################################

namespace eval UNAMES {

variable Dev

proc unames_init { dtype { stype "" } } {

	variable Dev

	# device layout type: "L" (Linux), other "Windows"
	set Dev(DEV) $dtype
	# system type: "L" (Linux), other "Windows/Cygwin"
	set Dev(SYS) $stype

	if { $Dev(DEV) == "L" } {
		# determine the root of virtual ttys
		if [file isdirectory "/dev/pts"] {
			# BSD style
			set Dev(PRT) "/dev/pts/%"
		} else {
			set Dev(PRT) "/dev/pty%"
		}
		# number bounds (inclusive) for virtual devices
		set Dev(PRB) { 0 8 }
		# real ttys
		if { $Dev(SYS) == "L" } {
			# actual Linux
			set Dev(RRT) "/dev/ttyUSB%"
		} else {
			# Cygwin
			set Dev(RRT) "/dev/ttyS%"
		}
		# and their bounds
		set Dev(RRB) { 0 255 }
	} else {
		set Dev(PRT) { "CNCA%" "CNCB%" }
		set Dev(PRB) { 0 3 }
		set Dev(RRT) "COM%:"
		set Dev(RRB) { 1 256 }
	}

	unames_defnames
}

proc unames_defnames { } {
#
# Generate the list of default names
#
	variable Dev

	# flag == default names, not real devices
	set Dev(DEF) 1
	# true devices
	set Dev(COM) ""
	# virtual devices
	set Dev(VCM) ""

	set rf [lindex $Dev(RRB) 0]
	set rt [lindex $Dev(RRB) 1]
	set pf [lindex $Dev(PRB) 0]
	set pt [lindex $Dev(PRB) 1]

	while { $rf <= $rt } {
		foreach d $Dev(RRT) {
			regsub "%" $d $rf d
			lappend Dev(COM) $d
		}
		incr rf
	}

	while { $pf <= $pt } {
		foreach d $Dev(PRT) {
			regsub "%" $d $pf d
			lappend Dev(VCM) $d
		}
		incr pf
	}
}

proc unames_ntodev { n } {
#
# Proposes a device list for a number
#
	variable Dev

	regsub -all "%" $Dev(RRT) $n d
	return $d
}

proc unames_ntovdev { n } {
#
# Proposes a virtual device list for a number
#
	variable Dev

	regsub -all "%" $Dev(PRT) $n d

	if { $Dev(DEV) == "L" && ![file exists $d] } {
		# this is supposed to be authoritative
		return ""
	}
	return $d
}

proc unames_unesc { dn } {
#
# Escapes the device name so it can be used as an argument to open
#
	variable Dev

	if { $Dev(DEV) == "L" } {
		# no need to do anything
		return $dn
	}

	if [regexp -nocase "^com(\[0-9\]+):$" $dn jk pn] {
		set dn "\\\\.\\COM$pn"
	} else {
		set dn "\\\\.\\$dn"
	}

	return $dn
}

proc unames_scan { } {
#
# Scan actual devices
#
	variable Dev

	set Dev(DEF) 0
	set Dev(COM) ""
	set Dev(VCM) ""

	# real devices
	for { set i 0 } { $i < 256 } { incr i } {
		set dl [unames_ntodev $i]
		foreach d $dl {
			if [catch { open [unames_unesc $d] "r" } fd] {
				continue
			}
			catch { close $fd }
			lappend Dev(COM) $d
		}
	}

	for { set i 0 } { $i < 32 } { incr i } {
		set dl [unames_ntovdev $i]
		if { $dl == "" } {
			continue
		}
		if { $Dev(DEV) == "L" } {
			# don't try to open them; unames_ntovdev is
			# authoritative, and opening those representing
			# terminals may mess them up
			foreach d $dl {
				lappend Dev(VCM) $d
			}
			continue
		}
		foreach d $dl {
			if [catch { open [unames_unesc $d] "r" } fd] {
				continue
			}
			catch { close $fd }
			lappend Dev(VCM) $d
		}
	}
}

proc unames_fnlist { fn } {
#
# Returns the list of filenames to try to open, given an element from one of
# the lists; if not on the list, assume a direct name (to be escaped, however)
#
	variable Dev

	if [regexp "^\[0-9\]+$" $fn] {
		# just a number
		return [unames_ntodev $fn]
	}

	if { [lsearch -exact $Dev(COM) $fn] >= 0 } {
		if !$Dev(DEF) {
			# this is an actual device
			return $fn
		}
		# get a number and convert to a list
		if ![regexp "\[0-9\]+" $fn n] {
			return ""
		}
		return [unames_ntodev $n]
	}
	if { [lsearch -exact $Dev(VCM) $fn] >= 0 } {
		if !$Dev(DEF) {
			return $fn
		}
		if ![regexp "\[0-9\]+" $fn n] {
			return ""
		}
		return [unames_ntovdev $n]
	}
	# return as is
	return $fn
}

proc unames_choice { } {

	variable Dev

	return [list $Dev(COM) $Dev(VCM)]
}

namespace export unames_*

### end of UNAMES namespace ###################################################
}

namespace import ::UNAMES::*

###############################################################################

package provide autoconnect 1.0

package require unames

###############################################################################
# Autoconnect #################################################################
###############################################################################

namespace eval AUTOCONNECT {

variable ACB

array set ACB { CLO "" CBK "" }

proc gize { fun } {

	if { $fun != "" && [string range $fun 0 1] != "::" } {
		set fun "::$fun"
	}

	return $fun
}

proc lize { fun } {

	return "::AUTOCONNECT::$fun"
}

proc autocn_heartbeat { } {
#
# Called to provide connection heartbeat, typically after a message reception
#
	variable ACB

	incr ACB(LRM)
}

proc autocn_start { op cl hs hc cc { po "" } { dl "" } } {
#
# op - open function
# cl - close function
# hs - command to issue handshake message
# hc - handshake condition (if true, handshake has been successful)
# cc - connection condition (if false, connection has been broken)
# po - poll function (to kick the node if no heartbeat)
# dl - explicit device list
# 
	variable ACB

	set ACB(OPE) [gize $op]
	set ACB(CLO) [gize $cl]
	set ACB(HSH) [gize $hs]
	set ACB(HSC) [gize $hc]
	set ACB(CNC) [gize $cc]
	set ACB(POL) [gize $po]
	set ACB(DLI) $dl

	# trc "ACN START: $ACB(OPE) $ACB(CLO) $ACB(HSH) $ACB(HSC) $ACB(CNC) $ACB(POL) $ACB(DLI)"

	# last device scan time
	set ACB(LDS) 0

	# automaton state
	set ACB(STA) "L"

	# hearbeat variables
	set ACB(RRM) -1
	set ACB(LRM) -1

	if { $ACB(CBK) != "" } {
		# a precaution
		catch { after cancel $ACB(CBK) }
	}
	set ACB(CBK) [after 10 [lize ac_callback]]
}

proc autocn_stop { } {
#
# Stop the callback and disconnect
#
	variable ACB

	if { $ACB(CBK) != "" } {
		catch { after cancel $ACB(CBK) }
		set ACB(CBK) ""
	}

	if { $ACB(CLO) != "" } {
		$ACB(CLO)
	}
}

proc ac_again { d } {

	variable ACB

	set ACB(CBK) [after $d [lize ac_callback]]
}

proc ac_callback { } {
#
# This callback checks if we are connected and if not tries to connect us
# automatically to the node
#
	variable ACB

	# trc "AC S=$ACB(STA)"
	if { $ACB(STA) == "R" } {
		# CONNECTED
		if ![$ACB(CNC)] {
			# just got disconnected
			set ACB(STA) "L"
			ac_again 100
			return
		}
		# we are connected, check for heartbeat
		if { $ACB(RRM) == $ACB(LRM) } {
			# stall?
			if { $ACB(POL) != "" } { $ACB(POL) }
			set ACB(STA) "T"
			set ACB(FAI) 0
			ac_again 1000
			return
		}
		set ACB(RRM) $ACB(LRM)
		# we are still connected, no need to panic
		ac_again 2000
		return
	}

	if { $ACB(STA) == "T" } {
		# HEARTBEAT FAILURE
		if ![$ACB(CNC)] {
			# disconnected
			set ACB(STA) "L"
			ac_again 100
			return
		}
		if { $ACB(RRM) == $ACB(LRM) } {
			# count failures
			if { $ACB(FAI) == 5 } {
				set ACB(STA) "L"
				$ACB(CLO)
				ac_again 100
				return
			}
			incr ACB(FAI)
			if { $ACB(POL) != "" } { $ACB(POL) }
			ac_again 600
			return
		}
		set ACB(RRM) $ACB(LRM)
		set ACB(STA) "R"
		ac_again 2000
		return
	}
	
	#######################################################################

	if { $ACB(STA) == "L" } {
		# CONNECTING
		set tm [clock seconds]
		if { $tm > [expr $ACB(LDS) + 5] } {
			# last scan for new devices was done more than 5 sec
			# ago, rescan
			if { $ACB(DLI) != "" } {
				set ACB(DVS) ""
				foreach d $ACB(DLI) {
					if [catch { expr $d } n] {
						lappend ACB(DVS) $d
					} else {
						lappend ACB(DVS) \
							[unames_ntodev $d]
					}
				}
			} else {
				unames_scan
				set ACB(DVS) [lindex [unames_choice] 0]
			}
			set ACB(DVL) [llength $ACB(DVS)]
			set ACB(LDS) $tm
			# trc "AC RESCAN: $ACB(DVS), $ACB(DVL)"
		}
		# index into the device table
		set ACB(CUR) 0
		set ACB(STA) "N"
		ac_again 250
		return
	}

	#######################################################################

	if { $ACB(STA) == "N" } {
		# TRYING NEXT DEVICE
		# trc "AC N CUR=$ACB(CUR), DVL=$ACB(DVL)"
		# try to open a new UART
		if { $ACB(CUR) >= $ACB(DVL) } {
			set ACB(STA) "L"
			if { $ACB(DVL) == 0 } {
				# no devices
				set ACB(LDS) 0
				ac_again 1000
				return
			}
			ac_again 100
			return
		}

		set dev [lindex $ACB(DVS) $ACB(CUR)]
		incr ACB(CUR)
		if { [$ACB(OPE) $dev] == 0 } {
			ac_again 100
			return
		}

		$ACB(HSH)

		set ACB(STA) "C"
		ac_again 2000
		return
	}

	if { $ACB(STA) == "C" } {
		# WAITING FOR HANDSHAKE
		if [$ACB(HSC)] {
			# established, assume connection OK
			set ACB(STA) "R"
			ac_again 2000
			return
		}
		# sorry, try another one
		# trc "AC C -> CLOSING"
		$ACB(CLO)
		set ACB(STA) "N"
		ac_again 100
		return
	}

	set ACB(STA) "L"
	ac_again 1000
}

namespace export autocn_*

}

namespace import ::AUTOCONNECT::*

###############################################################################

package provide noss 1.0

###############################################################################
# NOSS ########################################################################
###############################################################################

# This is N-mode packet interface with the Network ID field used as part
# of payload (akin to the boss package)

namespace eval NOSS {

###############################################################################
# ISO 3309 CRC + supplementary stuff needed by the protocol module ############
###############################################################################

variable CRCTAB {
    0x0000  0x1021  0x2042  0x3063  0x4084  0x50a5  0x60c6  0x70e7
    0x8108  0x9129  0xa14a  0xb16b  0xc18c  0xd1ad  0xe1ce  0xf1ef
    0x1231  0x0210  0x3273  0x2252  0x52b5  0x4294  0x72f7  0x62d6
    0x9339  0x8318  0xb37b  0xa35a  0xd3bd  0xc39c  0xf3ff  0xe3de
    0x2462  0x3443  0x0420  0x1401  0x64e6  0x74c7  0x44a4  0x5485
    0xa56a  0xb54b  0x8528  0x9509  0xe5ee  0xf5cf  0xc5ac  0xd58d
    0x3653  0x2672  0x1611  0x0630  0x76d7  0x66f6  0x5695  0x46b4
    0xb75b  0xa77a  0x9719  0x8738  0xf7df  0xe7fe  0xd79d  0xc7bc
    0x48c4  0x58e5  0x6886  0x78a7  0x0840  0x1861  0x2802  0x3823
    0xc9cc  0xd9ed  0xe98e  0xf9af  0x8948  0x9969  0xa90a  0xb92b
    0x5af5  0x4ad4  0x7ab7  0x6a96  0x1a71  0x0a50  0x3a33  0x2a12
    0xdbfd  0xcbdc  0xfbbf  0xeb9e  0x9b79  0x8b58  0xbb3b  0xab1a
    0x6ca6  0x7c87  0x4ce4  0x5cc5  0x2c22  0x3c03  0x0c60  0x1c41
    0xedae  0xfd8f  0xcdec  0xddcd  0xad2a  0xbd0b  0x8d68  0x9d49
    0x7e97  0x6eb6  0x5ed5  0x4ef4  0x3e13  0x2e32  0x1e51  0x0e70
    0xff9f  0xefbe  0xdfdd  0xcffc  0xbf1b  0xaf3a  0x9f59  0x8f78
    0x9188  0x81a9  0xb1ca  0xa1eb  0xd10c  0xc12d  0xf14e  0xe16f
    0x1080  0x00a1  0x30c2  0x20e3  0x5004  0x4025  0x7046  0x6067
    0x83b9  0x9398  0xa3fb  0xb3da  0xc33d  0xd31c  0xe37f  0xf35e
    0x02b1  0x1290  0x22f3  0x32d2  0x4235  0x5214  0x6277  0x7256
    0xb5ea  0xa5cb  0x95a8  0x8589  0xf56e  0xe54f  0xd52c  0xc50d
    0x34e2  0x24c3  0x14a0  0x0481  0x7466  0x6447  0x5424  0x4405
    0xa7db  0xb7fa  0x8799  0x97b8  0xe75f  0xf77e  0xc71d  0xd73c
    0x26d3  0x36f2  0x0691  0x16b0  0x6657  0x7676  0x4615  0x5634
    0xd94c  0xc96d  0xf90e  0xe92f  0x99c8  0x89e9  0xb98a  0xa9ab
    0x5844  0x4865  0x7806  0x6827  0x18c0  0x08e1  0x3882  0x28a3
    0xcb7d  0xdb5c  0xeb3f  0xfb1e  0x8bf9  0x9bd8  0xabbb  0xbb9a
    0x4a75  0x5a54  0x6a37  0x7a16  0x0af1  0x1ad0  0x2ab3  0x3a92
    0xfd2e  0xed0f  0xdd6c  0xcd4d  0xbdaa  0xad8b  0x9de8  0x8dc9
    0x7c26  0x6c07  0x5c64  0x4c45  0x3ca2  0x2c83  0x1ce0  0x0cc1
    0xef1f  0xff3e  0xcf5d  0xdf7c  0xaf9b  0xbfba  0x8fd9  0x9ff8
    0x6e17  0x7e36  0x4e55  0x5e74  0x2e93  0x3eb2  0x0ed1  0x1ef0
}

variable B

# abort function (in case of internal fatal error, considered impossible)
set B(ABR) ""

# diag output function
set B(DGO) ""

# character zero (aka NULL)
set B(ZER) [format %c [expr 0x00]]

# preamble byte
set B(IPR) [format %c [expr 0x55]]

# diag preamble (for packet modes) = ASCII DLE
set B(DPR) [format %c [expr 0x10]]

# reception automaton state
set B(STA) 0

# reception automaton remaining byte count
set B(CNT) 0

# function to call on packet reception
set B(DFN) ""

# function to call on UART close (which can happen asynchronously)
set B(UCF) ""

# low-level reception timer
set B(TIM)  ""

# packet timeout (msec), once reception has started
set B(PKT) 8000

###############################################################################

proc no_chks { wa } {

	variable CRCTAB

	set nb [string length $wa]

	set chs 0

	while { $nb > 0 } {

		binary scan $wa su waw
		#set waw [expr $waw & 0x0000ffff]

		set wa [string range $wa 2 end]
		incr nb -2

		set chs [expr (($chs << 8) ^ \
		    ( [lindex $CRCTAB [expr ($chs >> 8) ^ ($waw >>   8)]] )) & \
			0x0000ffff ]
		set chs [expr (($chs << 8) ^ \
		    ( [lindex $CRCTAB [expr ($chs >> 8) ^ ($waw & 0xff)]] )) & \
			0x0000ffff ]
	}

	return $chs
}

proc no_abort { msg } {

	variable B

	if { $B(ABR) != "" } {
		$B(ABR) $msg
		exit 1
	}

	catch { puts stderr $msg }
	exit 1
}

proc no_diag { } {

	variable B

	if { [string index $B(BUF) 0] == $B(ZER) } {
		# binary
		set ln [string range $B(BUF) 3 5]
		binary scan $ln cuSu lv code
		set ln "\[[format %02x $lv] -> [format %04x $code]\]"
	} else {
		# ASCII
		set ln "[string trim $B(BUF)]"
	}

	if { $B(DGO) != "" } {
		$B(DGO) $ln
	} else {
		puts "DIAG: $ln"
		flush stdout
	}
}

proc gize { fun } {

	if { $fun != "" && [string range $fun 0 1] != "::" } {
		set fun "::$fun"
	}

	return $fun
}

proc lize { fun } {

	return "::NOSS::$fun"
}

proc no_emu_readable { fun } {
#
# Emulates auto read on readable UART
#
	variable B

	if [$fun] {
		# a void call, increase the timeout
		if { $B(ROT) < $B(RMX) } {
			incr B(ROT)
		}
	} else {
		set B(ROT) 0
	}

	set B(ONR) [after $B(ROT) "[lize no_emu_readable] $fun"]
}

proc no_write { msg } {
#
# Writes a packet to the UART
#
	variable B

	set ln [string length $msg]
	if { $ln > $B(MPL) } {
		# truncate the message to size, probably a bad idea
		set ln $B(MPL)
		set msg [string range $msg 0 [expr $ln - 1]]
	}

	if [expr $ln & 1] {
		# need a filler zero byte
		append msg $B(ZER)
		incr ln -1
	} else {
		incr ln -2
	}

	if [catch {
		puts -nonewline $B(SFD) \
			"$B(IPR)[binary format c $ln]$msg[binary format s\
				[no_chks $msg]]"
		flush $B(SFD)
	}] {
		noss_close "NOSS write error"
	}
}

proc no_timeout { } {

	variable B

	if { $B(TIM) != "" } {
		no_rawread 1
		set B(TIM) ""
	}
}

proc no_rawread { { tm 0 } } {
#
# Called whenever data is available on the UART; returns 1 (if void), 0 (if
# progress, i.e., some data was available)
#
	variable B
#
#  STA = 0  -> Waiting for preamble
#        1  -> Waiting for the length byte
#        2  -> Waiting for (CNT) bytes until the end of packet
#        3  -> Waiting for end of DIAG preamble
#        4  -> Waiting for EOL until end of DIAG
#        5  -> Waiting for (CNT) bytes until the end of binary diag
#
	set chunk ""
	set void 1

	while 1 {

		if { $chunk == "" } {

			if [catch { read $B(SFD) } chunk] {
				# nonblocking read, ignore errors
				set chunk ""
			}

			if { $chunk == "" } {
				# check for timeout
				if $tm {
					# reset
					set B(STA) 0
				} elseif { $B(STA) != 0 } {
					# something has started, set up timer,
					# if not running already
					if { $B(TIM) == "" } {
						set B(TIM) \
				            	    [after $B(PKT) \
							[lize no_timeout]]
					}
				}
				return $void
			}
			# there is something to process, cancel timeout
			if { $B(TIM) != "" } {
				catch { after cancel $B(TIM) }
				set B(TIM) ""
			}
			set void 0
		}

		set bl [string length $chunk]

		switch $B(STA) {

		0 {
			# Look up the preamble byte in the received string
			for { set i 0 } { $i < $bl } { incr i } {
				set c [string index $chunk $i]
				if { $c == $B(IPR) } {
					# preamble found
					set B(STA) 1
					break
				}
				if { $c == $B(DPR) } {
					# diag preamble
					set B(STA) 3
					break
				}
			}
			if { $i == $bl } {
				# not found, keep waiting
				set chunk ""
				continue
			}
			# found, remove the parsed portion and keep going
			set chunk [string range $chunk [expr $i + 1] end]
		}

		1 {
			# expecting the length byte (note that the byte
			# does not cover the statid field, so its range is
			# up to MPL - 2)
			binary scan [string index $chunk 0] cu bl
			set chunk [string range $chunk 1 end]
			if { [expr $bl & 1] || $bl > [expr $B(MPL) - 2] } {
				# reset
				set B(STA) 0
				continue
			}
			# how many bytes to expect
			set B(CNT) [expr $bl + 4]
			set B(BUF) ""
			# found
			set B(STA) 2
		}

		2 {
			# packet reception, filling the buffer
			if { $bl < $B(CNT) } {
				append B(BUF) $chunk
				set chunk ""
				incr B(CNT) -$bl
				continue
			}

			# end of packet, reset
			set B(STA) 0

			if { $bl == $B(CNT) } {
				append B(BUF) $chunk
				set chunk ""
				# we have a complete buffer
				no_receive
				continue
			}

			# merged packets
			append B(BUF) [string range $chunk 0 [expr $B(CNT) - 1]]
			set chunk [string range $chunk $B(CNT) end]
			no_receive
		}

		3 {
			# waiting for the end of a diag header
			set chunk [string trimleft $chunk $B(DPR)]
			if { $chunk != "" } {
				set B(BUF) ""
				# look at the first byte of diag
				if { [string index $chunk 0] == $B(ZER) } {
					# a binary diag, length == 7
					set B(CNT) 7
					set B(STA) 5
				} else {
					# ASCII -> wait for NL
					set B(STA) 4
				}
			}
		}

		4 {
			# waiting for NL ending a diag
			set c [string first "\n" $chunk]
			if { $c < 0 } {
				append B(BUF) $chunk
				set chunk ""
				continue
			}

			append B(BUF) [string range $chunk 0 $c]
			set chunk [string range $chunk [expr $c + 1] end]
			# reset
			set B(STA) 0
			no_diag
		}

		5 {
			# waiting for CNT bytes of binary diag
			if { $bl < $B(CNT) } {
				append B(BUF) $chunk
				set chunk ""
				incr B(CNT) -$bl
				continue
			}
			# reset
			set B(STA) 0
			append B(BUF) [string range $chunk 0 [expr $B(CNT) - 1]]
			set chunk [string range $chunk $B(CNT) end]
			no_diag
		}

		default {
			set B(STA) 0
		}
		}
	}
}

proc no_receive { } {
#
# Handle a received packet
#
	variable B
	
	# dmp "RCV" $B(BUF)

	# validate CRC
	if [no_chks $B(BUF)] {
		return
	}

	# strip off the checksum
	set msg [string range $B(BUF) 0 end-2]
	set len [string length $msg]

	if { $len < 2 } {
		# ignore it
		return
	}

	if { $B(DFN) != "" } {
		$B(DFN) $msg
	}
}

proc noss_init { ufd mpl { inp "" } { dia "" } { clo "" } { emu 0 } } {
#
# Initialize: 
#
#	ufd - UART descriptor
#	mpl - max packet length
#	inp - function to be called on user input
#	dia - function to be called to present a diag message
#	clo - function to call on UART close (can happen asynchronously)
#	emu - emulate 'readable'
#
	variable B

	set B(STA) 0
	set B(CNT) 0
	set B(BUF) ""

	set B(SFD) $ufd
	set B(MPL) $mpl

	set B(UCF) $clo

	set B(DFN) $inp
	set B(DGO) $dia

	fconfigure $B(SFD) -buffering full -translation binary

	if $emu {
		# the readable flag doesn't work for UART on some Cygwin
		# setups
		set B(ROT) 1
		set B(RMX) $emu
		no_emu_readable [lize no_rawread]
	} else {
		# do it the easy way
		fileevent $B(SFD) readable [lize no_rawread]
	}
}

proc noss_stop { } {
#
# Stop the protocol
#
	variable B

	if { $B(TIM) != "" } {
		# kill the callback
		catch { after cancel $B(TIM) }
		set B(TIM) ""
	}

	set B(STA) 0
	set B(CNT) 0
	set B(BUF) ""
}

proc noss_close { { err "" } } {
#
# Close the UART (externally or internally, which can happen asynchronously)
#
	variable B

	# trc "NOSS CLOSE"

	if { [info exist B(ONR)] && $B(ONR) != "" } {
		# we have been emulating 'readable', kill the callback
		catch { after cancel $B(ONR) }
		unset B(ONR)
	}

	if { $B(UCF) != "" } {
		# any extra function to call?
		set bucf $B(UCF)
		# prevents recursion loops
		set B(UCF) ""
		$bucf $err
	}

	catch { close $B(SFD) }

	set B(SFD) ""

	# stop the protocol
	noss_stop

	set B(DFN) ""
	set B(DGO) ""
}

proc noss_send { buf } {
#
# This is the user-level output function
#
	variable B

	if { $B(SFD) == "" } {
		# ignore if disconnected, this shouldn't happen
		return
	}

	# dmp "SND" $buf

	no_write $buf
}

namespace export noss_*

}

namespace import ::NOSS::*

###############################################################################
###############################################################################

set OSST(word) 	[list 2 "su" 0 65535]
set OSST(sint) 	[list 2 "s" -32768 32767]
set OSST(lword) [list 4 "iu" 0 4294967295]
set OSST(lint) 	[list 4 "i" -2147483648 2147483647]
set OSST(byte) 	[list 1 "cu" 0 255]
set OSST(char) 	[list 1 "c" -128 127]
set OSST(blob) 	[list 2 ""]

proc oss_ierr { msg } {

	error $msg
}

proc oss_isalnum { txt } {

	return [regexp {^[[:alpha:]_][[:alnum:]_]*$} $txt]
}

proc process_struct { struct } {
#
# Parses the structure (command or message)
#
	global OSST

	set len 0
	set stl ""
	set blof 0

	# remove comments
	regsub -line -all {^[[:blank:]]*#.*} $struct "" struct

	while 1 {

		set struct [string trimleft $struct]
		if { $struct == "" } {
			break
		}

		if ![regexp {^([[:alpha:]]+)[[:space:]]+} $struct mat tp] {
			error "illegal attribute syntax: [truncmt $struct]"
		}

		if ![info exists OSST($tp)] {
			error "unknown type, must be one of: [array names OSST]"
		}

		# remove the match
		set struct [string range $struct [string length $mat] end]

		if { $tp == "blob" } {
			if $blof {
				error "you cannot have more than one blob"
			}
			set blof 1
		} else {
			if $blof {
				error "a blob can only occur as the last\
					attribute"
			}
		}

		# size
		set tis [lindex $OSST($tp) 0]

		while { $tis > 1 && [expr $len % $tis] } {
			# alignment
			incr len
		}

		# parse the name
		if ![regexp {^([[:alpha:]_][[:alnum:]_]+)} $struct nm] {
			error "illegal attribute syntax, name expected\
				after $tp: [truncmt $struct]"
		}

		if [info exists nms($nm)] {
			error "duplicate attribute name: $nm"
		}

		# remove the match
		set struct [string range $struct [string length $nm] end]

		set nms($nm) ""

		set struct [string trimleft $struct]
		set cc [string index $struct 0]

		if { $cc == "\[" } {
			if { $tp == "blob" } {
				error "blob cannot be dimensioned"
			}
			if ![regexp {^.([^;\[]+)\]} $struct mat ex] {
				error "illegal dimension for $nm:\
					[truncmt $struct]"
			}
			# remove the match
			set struct [string range $struct [string length $mat] \
				end]

			if { [catch { expr $ex } dim] || \
			    [catch { oss_valint $dim 1 256 } dim] } {
				error "illegal dimension for $nm, $dim"
			}

			set struct [string trimleft $struct]
			set cc [string index $struct 0]
			set cnt $dim
		} else {
			set cnt 1
			set dim 0
		}

		if { $cc != ";" } {
			error "semicolon missing at the end of $nm\
				specification"
		}

		set struct [string range $struct 1 end]

		# offset length count type name
		lappend stl [list $tp $nm $tis $dim $len]
		incr len [expr $tis * $cnt]
	}

	return [list $stl $len]
}

proc oss_blobtovalues { blob } {
#
# Convert a blob to a list of numerical values
#
	if { [string length $blob] < 2 } {
		error "blob missing"
	}

	binary scan $blob su size

	if { $size == 0 } {
		return ""
	}

	set blob [string range $blob 2 [expr $size + 1]]
	if { [string length $blob] != $size } {
		error "blob data too short, $size bytes expected"
	}

	set res ""

	for { set i 0 } { $i < $size } { incr i } {
		binary scan [string index $blob $i] cu val
		lappend res "0x[format %02X $val]"
	}

	return $res
}

proc oss_blobtostring { blob } {
#
# Convert a blob to a string
#
	if { [string length $blob] < 2 } {
		error "blob missing"
	}

	binary scan $blob su size

	if { $size == 0 } {
		return ""
	}

	set blob [string range $blob 2 [expr $size + 1]]
	if { [string length $blob] != $size } {
		error "blob data too short, $size bytes expected"
	}

	# remove the sentinel(s) on the right
	return [string trimright $blob [binary format c 0]]
}

proc oss_valint { n { min "" } { max "" } } {
#
# Validate an integer value
#
	set n [string tolower [string trim $n]]
	if { $n == "" } {
		error "empty string"
	}

	if { [string first "." $n] >= 0 || [string first "e" $n] >= 0 } {
		error "not an integer number"
	}

	if [catch { expr $n } n] {
		error "not a number"
	}

	if { $min != "" && $n < $min } {
		error "must not be less than $min"
	}

	if { $max != "" && $n > $max } {
		error "must not be greater than $max"
	}

	return $n
}

proc oss_command { name code struct } {
#
# Declares a command layout
#
	global PM OSSCC OSSCN

	if ![oss_isalnum $name] {
		oss_ierr "oss_command name ($name) should be alphanumeric"
		return
	}

	if [info exists OSSCN($name)] {
		oss_ierr "oss_command, duplicate command name $name"
		return
	}

	if [catch { oss_valint $code 1 255 } cc] {
		oss_ierr "oss_command $name, invalid code, $cc"
		return
	}

	if [info exists OSSCC($cc)] {
		oss_ierr "oss_command $name, duplicate code $code"
	}

	if [catch { process_struct $struct } str] {
		oss_ierr "oss_command $name, $str"
		return
	}

	set OSSCN($name) [list $name $str $code]

	# check if the length is within limits
	set len [lindex $str 1]

	# effective length of the command body (struct)
	set efl [expr { $PM(MPL) - 4 }]

	if { $len > $efl } {
		oss_ierr "oss_command $name, command structure too long: $len,\
			the limit (implied by the -length attribute of\
			oss_interface) is $efl"
	}
			
	set OSSCC($cc) $OSSCN($name)
}

proc oss_message { name code struct } {
#
# Declares a message layout
#
	global PM OSSMC OSSMN

	if ![oss_isalnum $name] {
		oss_ierr "oss_message name ($name) should be alphanumeric"
		return
		
	}

	if [info exists OSSMN($name)] {
		oss_ierr "oss_message, duplicate message name $name"
		return
	}

	if [catch { oss_valint $code 1 255 } cc] {
		oss_ierr "oss_message $name, invalid code, $cc"
		return
	}

	if [info exists OSSMC($cc)] {
		oss_ierr "oss_message $name, duplicate code $code"
	}

	if [catch { process_struct $struct } str] {
		oss_ierr "oss_message $name, $str"
		return
	}

	# check if the length is within limits
	set len [lindex $str 1]

	# effective length of the message body (struct)
	set efl [expr $PM(MPL) - 2]

	if { $len > $efl } {
		oss_ierr "oss_message $name, message structure too long: $len,\
			the limit (implied by the -length attribute of\
			oss_interface) is $efl"
	}
	
	set OSSMN($name) [list $name $str $code]
	set OSSMC($cc) $OSSMN($name)
}

proc oss_bytestobin { bytes } {

	set res ""

	for { set i 0 } { $i < [llength $bytes] } { incr i } {
		set vv [lindex $bytes $i]
		if [catch { expr $vv & 0xFF } val] {
			error "illegal value in list of bytes, $vv"
		}
		append res [binary format c $val]
	}

	return $res
}

proc oss_valuestoblob { vals } {
#
# Convert numerical values to a blob
#
	set cnt [llength $vals]

	if { $cnt > 65535 } {
		set cnt 65535
		set vals [lrange $vals 0 65534]
	}

	set res "[binary format s $cnt][oss_bytestobin $vals]"

	return $res
}

proc oss_stringtoblob { vals } {

	set zer [binary format c 0]
	if { [string index $vals end] != $zer } {
		append vals $zer
	}

	set len [string length $vals]
	if { $len > 65535 } {
		set len 65535
		set vals "[string range $vals 0 65533]$zer"
	}

	return "[binary format s $len]$vals"
}

proc oss_getcmdstruct { nc { nam "" } { cod "" } { len "" } } {
#
# Get the command structure by name or code
#
	global OSSCC OSSCN

	if [info exists OSSCN($nc)] {
		set c $OSSCN($nc)
	} elseif [info exists OSSCC($nc)] {
		set c $OSSCC($nc)
	} else {
		return ""
	}

	if { $nam != "" } {
		upvar $nam n
		set n [lindex $c 0]
	}

	if { $cod != "" } {
		upvar $cod k
		set k [lindex $c 2]
	}

	set c [lindex $c 1]

	if { $len != "" } {
		upvar $len l
		set l [lindex $c 1]
	}

	return [lindex $c 0]
}

proc oss_getmsgstruct { nc { nam "" } { cod "" } { len "" } } {
#
# Get the message structure by name or code
#
	global OSSMC OSSMN

	if [info exists OSSMN($nc)] {
		set c $OSSMN($nc)
	} elseif [info exists OSSMC($nc)] {
		set c $OSSMC($nc)
	} else {
		return ""
	}

	if { $nam != "" } {
		upvar $nam n
		set n [lindex $c 0]
	}

	if { $cod != "" } {
		upvar $cod k
		set k [lindex $c 2]
	}

	set c [lindex $c 1]

	if { $len != "" } {
		upvar $len l
		set l [lindex $c 1]
	}

	return [lindex $c 0]
}

proc oss_setvalues { vals nc { bstr 0 } } {
#
# Constructs a command block from vals according to structure str, bstr as above
#
	global OSST

	set res ""
	set fil 0
	set zer [binary format c 0]

	set stru [oss_getcmdstruct $nc]

	if { $stru == "" } {
		error "command $nc not found"
	}

	while 1 {

		if { $vals == "" } {
			if { $stru != "" } {
				error "oss_setvalues, more structure attributes\
					than values"
			}
			break
		}

		if { $stru == "" } {
			error "oss_setvalues, more values than structure\
				attributes"
		}

		set val [lindex $vals 0]
		set str [lindex $stru 0]
		set vals [lrange $vals 1 end]
		set stru [lrange $stru 1 end]

		foreach { tp nm ts di of } $str { }

		while { $fil < $of } {
			# align as needed
			append res $zer
			incr fil
		}

		if { $tp == "blob" } {
			if $bstr {
				append res [oss_stringtoblob $val]
			} else {
				append res [oss_valuestoblob $val]
			}
			break
		}

		set fm [string index [lindex $OSST($tp) 1] 0]

		if $di {
			set dy $di

			while { $di } {
				if [catch { expr [lindex $val 0] } v] {
					error "illegal item in value list for\
					oss_setvalues: $val (should be a list\
					of $dy numbers)"
				}
				append res [binary format $fm $v]
				set val [lrange $val 1 end]
				incr di -1
				incr fil $ts
			}
		} else {
			if [catch { expr $val } v] {
				error "illegal item in value list for\
					oss_setvalues: $val (should be a\
					number)"
			}
			append res [binary format $fm $v]
			incr fil $ts
		}
	}

	return $res
}

proc oss_getvalues { blk nc { bstr 0 } } {
#
# Retrieve values from binary blk according to message nc, bstr means that
# a blob is to be treated as a string
#
	variable OSST

	set res ""

	set str [oss_getmsgstruct $nc]
	if { $str == "" } {
		error "message $nc not found"
	}

	while 1 {

		if { $str == "" } {
			return $res
		}

		foreach { tp nm ts di of } [lindex $str 0] { }
		set str [lrange $str 1 end]

		if { $tp == "blob" } {
			# this one is a bit special
			set b [string range $blk $of end]
			if $bstr {
				lappend res [oss_blobtostring $b]
			} else {
				lappend res [oss_blobtovalues $b]
			}
			return $res
		}

		# select the format based on type
		set fm [lindex $OSST($tp) 1]

		if { $di == 0 } {
			# a single item
			set b [string range $blk $of [expr $of + $ts - 1]]
			if { [string length $b] < $ts } {
				# truncated
				error "message shorter than structure,\
					attribute $nm missing"
			}
			binary scan $b $fm d
			lappend res $d
			continue
		}

		# an array
		set tre ""
		set siz [expr $di * $ts]
		set b [string range $blk $of [expr $of + $siz - 1]]
		if { [string length $b] < $siz } {
			# truncated
			error "message shorter than structure, attribute\
				$nm missing"
		}

		while { $di } {

			binary scan $b $fm d
			lappend tre $d
			set b [string range $b $ts end]
			incr di -1
		}

		lappend res $tre
	}
}

proc oss_issuecommand { code block } {

	global PM ST

	set msg "[binary format cc $code $ST(OPREF)]$block"

	set cl [string length $msg]
	if { $cl > $PM(MPL) } {
		error "command packet too long: $cl, the max is $PM(MPL)"
	}

	incr ST(OPREF)

	if { $ST(OPREF) >= 256 } {
		set ST(OPREF) 1
	}

	noss_send $msg
}

###############################################################################
###############################################################################

###############################################################################
###############################################################################
# Copied from praxis oss.tcl ##################################################
###############################################################################
###############################################################################

set ACKCODE(0)		"OK"
set ACKCODE(1)		"command format error"
set ACKCODE(2)		"illegal length of command packet"
set ACKCODE(3)		"illegal command parameter"
set ACKCODE(4)		"illegal command code"
set ACKCODE(6)		"module is off"
set ACKCODE(7)		"module is busy"
set ACKCODE(8)		"temporarily out of resources"
set ACKCODE(9)		"wrong module configuration"
set ACKCODE(10)		"void command"

# These are detected by the Peg (AP)

set ACKCODE(129) 	"command format error (detected by AP)"
set ACKCODE(130)	"command format error"
set ACKCODE(131)	"command too long"

proc myinit { } {

	global CTiming

	set CTiming [clock milliseconds]
}

myinit

#############################################################################
#############################################################################
##
## Commands, messages
##
#############################################################################
#############################################################################

oss_command config 0x01 {
#
# Sensor configuration; this is just a blob of blocks:
#
#	ssssllll	sss -> sensor number, lllll -> bytes that follow - 1
#	ppppvvvv	ppp -> parameter, vvvv -> value
#
	blob	confdata;
}

oss_command radio 0x02 {
#
# Query or set the device status
#
	byte	options;
}

oss_command status 0x03 {
#
# Get device status (no args)
#
	byte	dummy;
}

oss_command onoff 0x04 {
#
# Turn sensors on or off
#
	# zero == all 0x80 == on
	byte	which;
}

oss_command sample 0x05 {
#
# Start sampling
#
	# Samples per minute
	word	spm;
}

oss_command stream 0x06 {
#
# Start streaming
#
	blob	confdata;
}

oss_command stop 0x07 {
#
# Stop sampling
#
	byte	dummy;
}

oss_command ap 0x08 {
#
# Access point configuration
#
	# Node ID (setup Id)
	word	nodeid;
	# Packet retry count
	byte	nretr;
	# Packet loss rate (debugging)
	word	loss;
}

oss_command mreg 0x09 {
#
# Debugging
#
	byte	what;
	byte	regn;
	byte	value;
}

#############################################################################
#############################################################################

oss_message status 0x03 {
#
# Status info
#
	lword	uptime;
	lword	taken;
	word 	battery;
	word	freemem;
	word	minmem;
	word	rate;
	byte	sset;
	byte	status;
	# 21 bytes so far
	# sensor conf, all sensors, 17 nibbles, 9 bytes, total = 30 (39)
	blob	sstat;
}

oss_message report 0x05 {
#
# Sensor readings
#
	# sample number modulo 64K
	word	sample;
	# the layout:	bits 0-4   the present components of imu
	#		bits 5-6   the present components for humid
	#		bit  7     mic present
	#		bit  8     light present
	#		bits 9-10  the present components of pressure
	word	layout;
	blob	data;
}

oss_message motion 0x06 {
#
# Motion event report
#
	# event count
	word	events;
	# acceleration
	word	accel [3];
}

oss_message ap 0x08 {
#
# To be extended later
#
	word	nodeid;
	byte	nretr;
	word	loss;
}

oss_message mreg 0x09 {
	blob	data;
}

oss_message sblock 0x80 {
#
# A streaming block
#
	lword	data [12];
}

oss_message etrain 0x81 {
#
# End of train
#
	lword	last;
	word	offset;
	word	clock;
	byte	dropped;
}

##############################################################################
##############################################################################
# prepackaged commands #######################################################
##############################################################################
##############################################################################

proc get_rss { msg } {

	binary scan [string range $msg end end] cu rs

	return "RSS: $rs"
}

proc issue_start { } {
#
# Issue a command to start data collection
#
	# WOR wakeup
	oss_issuecommand 0x02 [oss_setvalues [list 2] "radio"]
	# handcraft the blob
	set bb ""
	lappend bb [expr { (0 << 4) | 5 }]
	lappend bb [expr { (0 << 4) | 4 }]
	lappend bb [expr { (2 << 4) | 4 }]
	lappend bb [expr { (3 << 4) | 4 }]
	lappend bb [expr { (4 << 4) | 4 }]
	lappend bb [expr { (5 << 4) | 1 }]
	lappend bb [expr { (7 << 4) | 5 }]
	oss_issuecommand 0x06 [oss_setvalues [list $bb] "stream"]
}

proc issue_stop { } {
#
# Stop data collection
#
	oss_issuecommand 0x07 [oss_setvalues [list 0] "stop"]
}

###############################################################################
###############################################################################
###############################################################################

proc clear_txt { w } {

	if [catch { $w configure -state normal } ] {
		return
	}

	$w delete 1.0 end
	$w configure -state disabled
}

proc cut_copy_paste { w x y { c "" } } {
#
# Handles windows-style cut-copy-paste from a text widget; invoked in response
# to right click in a text widget
#
	if [catch { $w get sel.first sel.last } sel] {
		# selection absent -> empty
		set sel ""
	}

	# determine the state, i.e., are we allowed to paste into the widget?
	set sta [$w cget -state]
	if { [string first "normal" $sta] >= 0 } {
		set sta "normal"
	} else {
		set sta "disabled"
	}

	set r $w._rcm

	catch { destroy $r }

	set m [menu $r -tearoff 0]

	if { $sel != "" && $sta == "normal" } {
		# cut allowed
		set st "normal"
	} else {
		set st "disabled"
	}
	$m add command -label "Cut" -command "tk_textCut $w" -state $st

	if { $sel != "" } {
		# copy allowed
		set st "normal"
	} else {
		set st "disabled"
	}
	$m add command -label "Copy" -command "tk_textCopy $w" -state $st

	if [catch { clipboard get -displayof $w } cs] {
		set cs ""
	}
	if { $sta == "normal" && $cs != "" } {
		set st "normal"
	} else {
		set st "disabled"
	}
	$m add command -label "Paste" -command "tk_textPaste $w" -state $st

	if { $c != "" } {
		$m add separator
		if [$w compare 1.0 < "end - 1 chars"] {
			set st "normal"
		} else {
			set st "disabled"
		}
		$m add command -label "Clear" -command "clear_txt $w" \
			-state $st
	}

	tk_popup $m $x $y
}

proc term_clean { } {

	global Term

	$Term configure -state normal
	$Term delete 1.0 end
	$Term configure -state disabled
}

proc term_addtxt { txt } {

	global Term

	$Term configure -state normal
	$Term insert end $txt
	$Term configure -state disabled
	$Term yview -pickplace end
}

proc term_endline { } {

	global Term PM

	$Term configure -state normal
	$Term insert end "\n"

	while 1 {
		set ix [$Term index end]
		set ix [string range $ix 0 [expr [string first "." $ix] - 1]]
		if { $ix <= $PM(TRL) } {
			break
		}
		# delete the topmost line if above limit
		$Term delete 1.0 2.0
	}

	$Term configure -state disabled
	# make sure the last line is displayed
	$Term yview -pickplace end
}

proc dsp { ln } {
#
# Write a line to the terminal; the tricky bit is that we cannot assume that
# the line doesn't contain newlines
#
	global ST

	if { $ST(SDS) != "" } {
		catch { puts $ST(SDS) $ln }
	}

	append ln "\r\n"
	while { [regexp "\[\r\n\]" $ln m] } {
		# eol delimiter
		set el [string first $m $ln]
		if { $el == 0 } {
			# first character, check the second one
			set n [string index $ln 1]
			if { $m == "\r" && $n == "\n" || \
			     $m == "\n" && $n == "\r"    } {
				# two-character EOL
				set ln [string range $ln 2 end]
			} else {
				set ln [string range $ln 1 end]
			}
			# complete previous line
			term_endline
			continue
		}
		# send the preceding string to the terminal
		term_addtxt [string range $ln 0 [expr $el - 1]]
		set ln [string range $ln $el end]
	}
}

###############################################################################

proc fparent { } {

	set w [focus]

	if { $w == "" } {
		return ""
	}

	return "-parent $w"
}


proc alert { msg } {

	tk_messageBox -type ok -default ok -message "ATTENTION!" -detail $msg \
		-icon warning {*}[fparent] -title "Attention"
}

proc mkRootWindow { } {

	global ST FFont Term WI

	wm title . "DOGS $ST(VER)"

	set w [frame .top]
	pack $w -side top -expand y -fill both

	set Term [text $w.t \
		-yscrollcommand "$w.scroly set" \
		-setgrid true \
		-width 80 -height 24 -wrap char \
		-font $FFont \
		-exportselection 1 \
		-state disabled]

	scrollbar $w.scroly -command "$Term yview"
	pack $w.scroly -side right -fill y
	pack $Term -side top -expand yes -fill both
	bind $Term <ButtonRelease-3> "cut_copy_paste %W %X %Y c"

	set w [frame .mid]
	pack $w -side top -expand y -fill x

	# the four mark buttons (why four?)
	foreach i { 0 1 2 } c { red yellow green } \
	    t { "MARK 1" "MARK 2" "MARK 3" } {
		set b [button $w.b$i -text $t -command "cbclick $i $c $t" \
			-bg $c]
		grid $b -column $i -row 0 -sticky news -padx 1 -pady 1
		grid columnconfigure $w $i -weight 1
	}

	set b [button $w.stop -text "STOP" -command "startstop 0" -width 14]
	grid $b -column 0 -row 1 -sticky news -padx 1 -pady 1
	set WI(STOP) $b

	set b [button $w.save -text "" -command "savelog" -width 14]
	grid $b -column 1 -row 1 -sticky news -padx 1 -pady 1
	set WI(SAVE) $b

	set b [button $w.strt -text "START" -command "startstop 1" -width 14]
	grid $b -column 2 -row 1 -sticky news -padx 1 -pady 1
	set WI(STRT) $b

	bind . <Destroy> "terminate"
}

proc savelog { } {

	global ST

	if { $ST(SDS) != "" } {
		# stop saving
		catch { close $ST(SDS) }
		set ST(SDS) ""
		updstatus
		return
	}

	set fp [getconf "SF"]

	while 1 {

		set fn [tk_getSaveFile \
			-defaultextension ".txt" \
			{*}[fparent] \
			-title "Save file name" \
			-initialfile [file tail $fp] \
			-confirmoverwrite false \
			-filetypes { { TEXT { .txt } } } \
			-initialdir [file dirname $fp]]

		if { $fn == "" } {
			# cancelled
			return
		}

		if [catch { open $fn "a" } fd] {
			alert "Cannot open file $fn, $fd"
			continue
		}

		setconf SF $fn

		set ST(SDS) $fd
		updstatus
		return
	}
}

proc cbclick { n c t } {

	dsp "EVENT: $n $c $t"
}

proc terminate { } {

	global ST

	if { $ST(REX) == 0 } {
		set ST(REX) 1
		puts "TERMINATING"
		exit 0
	}
}

###############################################################################

proc uart_open { udev } {

	global PM ST

	if { $ST(SYS) == "L" } {
		set accs { RDWR NOCTTY NONBLOCK }
	} else {
		set accs "r+"
	}

	set ST(UCS) $udev
	set d [unames_unesc $udev]

	if [catch { open $d $accs } ST(SFD)] {
		set ST(SFD) ""
		return 0
	}

	if [catch { fconfigure $ST(SFD) -mode "$PM(USP),n,8,1" -handshake none \
	    -blocking 0 -eofchar "" -ttycontrol { RTS 0 } } err] {
		catch { close $ST(SFD) }
		set ST(SFD) ""
		return 0
	}

	set ST(HSK) 0
	updstatus

	# configure the protocol
	noss_init $ST(SFD) $PM(MPL) uart_read ttyout uart_close $PM(EMU)

	return 1
}

proc uart_close { { err "" } } {

	global ST

	catch { close $ST(SFD) }
	set ST(SFD) ""
	set ST(HSK) 0

	updstatus
}

proc send_handshake { } {

	global PM

	set msg [binary format cci 0x00 0x00 $PM(PXI)]

	# twice to make it more reliable
	noss_send $msg
	noss_send $msg
}

proc handshake_ok { } {

	global ST
	return $ST(HSK)
}

proc connected { } {

	global ST
	return [expr { $ST(SFD) != "" }]
}

proc poll { } {

	global PM

	set msg [binary format cci 0x00 0x00 $PM(PXI)]
	noss_send $msg
}

proc uart_read { msg } {
#
# Handles data from the node
#
	global PM ST

	autocn_heartbeat

	set len [string length $msg]
	if { $len < 2 } {
		# no need to even look at it
		return
	}

	binary scan $msg cucu code opref
	set mes [string range $msg 2 end]

	if { $code == 0 && $opref == 0 } {
		# heartbeat/autoconnect
		if { $len < 4 } {
			# ignore
			return
		}
		binary scan $mes su pxi
		if { $pxi == [expr ($PM(PXI) ^ ($PM(PXI) >> 16)) & 0xFFFF] } {
			# handshake OK
			if !$ST(HSK) {
				set ST(HSK) 1
				updstatus
			}
		}
		return
	}

	# anything else must be expected
	if { $code == 0 } {

		global ACKCODE

		binary scan $mes su mes

		if [info exists ACKCODE($mes)] {
			set mes $ACKCODE($mes)
		}
		dsp "<$opref>: $mes"
		return
	}

	if { $code == 128 } {
		# report
		catch { handle_report $mes $opref }
		return
	}

	if { $code == 129 } {
		# EOT
		catch { handle_eot $mes $opref }
		return
	}

	# ... more?
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

proc update_state { } {

	global ST PM

	if !$ST(ACT) {
		# not expecting reports
		set sec [clock seconds]
		if { [expr { $sec - $ST(LAS) } ] >=
		    [expr { $PM(RET) / 1000 } ] } {
			# stop command
			issue_stop
			set ST(LAS) $sec
		}
	} else {
		# expecting reports
		if { $ST(SCB) != "" } {
			# start callback is active, abort it
			catch { after cancel $ST(SCB) }
			set ST(SCB) ""
		}
	}
}

proc handle_report { msg ref } {

	update_state

	# a single blob
	set dat [lindex [oss_getvalues $msg "sblock"] 0]

	set bn $ref
	set sh 8
	for { set i 0 } { $i < 12 } { incr i } {
		append fm "\n"
		set ci [lindex $dat $i]
		# turn them into 16-bit floats
		append fm [to_f16 [expr { ($ci >> 16) & 0xffc0 }]]
		append fm " "
		append fm [to_f16 [expr { ($ci >>  6) & 0xffc0 }]]
		append fm " "
		append fm [to_f16 [expr { ($ci <<  4) & 0xffc0 }]]
		set b [expr {  $ci        & 0x0003 }]
		set bn [expr { $bn |   ($b << $sh) }]
		incr sh 2
	}

	dsp "B: [format %10u $bn]$fm"
}

proc handle_eot { msg ref } {

	update_state

	lassign [oss_getvalues $msg "etrain"] last offset sclock drop

	dsp "E: [format %10u $last] [format %5u $offset]\
		 [format %5u $sclock] [format %3u $drop]"
}

proc startstop { on } {

	global ST

	if $on {
		if $ST(ACT) {
			dsp "ALREADY ON!"
			return
		}
		set ST(ACT) 1
		if { $ST(SCB) != "" } {
			# a precaution
			catch { after cancel $ST(SCB) }
		}
		updstatus
		startup_callback
		return
	}

	if !$ST(ACT) {
		dsp "ALREADY OFF"
		return
	}

	if { $ST(SCB) != "" } {
		catch { after cancel $ST(SCB) }
		set ST(SCB) ""
	}

	set ST(ACT) 0
	issue_stop
	#issue_stop
	#issue_stop
	updstatus
	set ST(LAS) [clock seconds]
}

proc startup_callback { } {

	global ST PM

	if $ST(ACT) {
		issue_start
		set ST(SCB) [after $PM(RET) startup_callback]
		return
	}

	set ST(SCB) ""
}

proc updstatus { } {

	global ST WI

	if $ST(ACT) {
		$WI(STRT) configure -state disabled
		$WI(STOP) configure -state normal
	} else {
		$WI(STOP) configure -state disabled
		$WI(STRT) configure -state normal
	}

	if { $ST(SDS) == "" } {
		$WI(SAVE) configure -text "SAVE TO FILE"
	} else {
		$WI(SAVE) configure -text "STOP SAVING"
	}

	dsp "STATUS: $ST(HSK) $ST(ACT) [connected]"
}

###############################################################################
###############################################################################

proc rcfdir { } {
#
# Produces the path to the application's "save" directory
#
	global env

	if [info exists env(APPDATA)] {
		# we are (probably) on Windows
		return [file join $env(APPDATA) "Olsonet" "ewe"]
	}

	if [info exists env(HOME)] {
		# we are (probably) on UNIX
		return [file join $env(HOME) ".olsonet" "ewe"]
	}

	# this directory
	return [pwd]
}

proc rcread { } {
#
# Reads the configuration file
#
	global ST CONF

	set fp [file join $ST(RCD) "config.txt"]
	set fd ""

	array unset CONF

	if { [catch { open $fp "r" } fd] || [catch { read $fd } pm] } {
		# cannot open or read, presume doesn't exist
		catch { close $fd }
		return
	}

	close $fd

	if [catch { array set CONF $pm }] {
		array unset CONF
	}
}

proc rcwrite { } {
#
# Writes the configuration file
#
	global CONF ST

	set fp [file join $ST(RCD) "config.txt"]

	# make sure all the directories exist
	catch { file mkdir [file dirname $fp] }

	if { [catch { open $fp "w" } fd] ||
	     [catch { puts -nonewline $fd [array get CONF] }] } {
		catch { close $fd }
	} else {
		close $fd
	}
}

proc setconf { par val } {

	global CONF

	set CONF($par) $val
	rcwrite
}

proc getconf { par } {

	global CONF

	if [info exists CONF($par)] {
		return $CONF($par)
	}

	return ""
}

###############################################################################

proc main { } {

	global ST

	set ST(RCD) [rcfdir]
	rcread

	unames_init $ST(DEV) $ST(SYS)

	mkRootWindow

	updstatus

	autocn_start \
		uart_open \
		noss_close \
		send_handshake \
		handshake_ok \
		connected \
		poll \
		""
}

main
vwait forever
