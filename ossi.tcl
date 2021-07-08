#
# Commands
#
#	configure sensor par ... par sensor par ... par ...
#	radio on|off|hibernate
#	radio wor offdely worinvl
#	on sensor ... sensor
#	off sensor ... sensor
#	sample -frequency freq -count cnt
#	status
#	stop
#	ap -node nn -wake nw -retries nr -preamble pl
#
#
#	Add packed acc data: 
#
#		sec mod 64K, smp mod 8, prc, ns
#		prc = 0,1,2,3 -> 16, 12, 10, 8
#		3x10 = 30 x 4 = 16 bytes (redundancy, compression?)

###############################################################################
# Configuration parameters ####################################################
###############################################################################
#
# Sensor names. This assigns numerical identifiers to the sensors in the listed
# order, starting from zero.
#
variable SNAMES
#
set SNAMES(S) 		{ "imu" "humidity" "microphone" "light" "pressure" }
# Single-letter abbreviations
set SNAMES(A)		"ihmlp"
#
# Discrete gradation for those parameters that fit the "g" class. Actual 
# numerical parameters will be mapped into this scale by the node. These
# are synonymes for 0-7.
#
set SNAMES(G) 		{ "tiny" "low" "small" "medium" "big" "high" "huge" 
				"extreme" }

#
# Components by sensor. Only for those sensors that have components.
#
variable COMPS
#
set COMPS(imu) 		"agct"
set COMPS(humidity)	"ht"
set COMPS(pressure)	"pt"

#
# Specific parameters by sensor: b = yes/no, g = graded, string = component
# selection
#
variable CPARAMS
#
set CPARAMS(imu)	{
				{ "motion" "threshold" "rate" "accuracy"
					"bandwidth" "components" "report" }
				{ b g g g g "agct" b }
			}
set CPARAMS(humidity)	{
				{ "heater" "accuracy" "components" "sampling" }
				{ b g "ht" g }
			}
set CPARAMS(microphone)	{
				{ "rate" }
				{ g }
			}
set CPARAMS(light)	{
				{ "continuous" "accuracy" "sampling" }
				{ b g g }
			}
set CPARAMS(pressure)	{
				{ "forced" "rate" "accuracy" "bandwidth"
					"components" "sampling" }
				{ b g g g "pt" g }
			}
#
# Convert sensor value to battery voltage
#
proc sensor_to_voltage { val } {

	# integer volts
	set g [expr { $val >> 8 }]
	set f [expr { double($val & 0xff) / 256.0 }]
	return [format %4.2f [expr { double($g) + $f }]]
}

###############################################################################
###############################################################################

variable ACKCODE

set ACKCODE(0)		"OK"
set ACKCODE(1)		"command format error"
set ACKCODE(2)		"illegal length of command packet"
set ACKCODE(3)		"illegal command parameter"
set ACKCODE(4)		"illegal command code"
set ACKCODE(6)		"module is off"
set ACKCODE(7)		"module is busy"
set ACKCODE(8)		"temporarily out of resources"

set ACKCODE(129) 	"command format error (detected by AP)"
set ACKCODE(130)	"command too long for RF"

#############################################################################
#############################################################################

oss_interface -id 0x00010022 -speed 115200 -length 56 \
	-parser { parse_cmd show_msg start_up }

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
#	0 - off, 1 - WOR, 2 - on, 3 - hibernate
	word	offdelay;
	word	worintvl;
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
	# Current time (to set)
	lword	seconds;
	# How many samples
	lword	nsamples;
	# Samples per minute
	word	spm;
	# Samples per packet
	word	spp;
}

oss_command stop 0x06 {
#
# Stop sampling
#
	byte	dummy;
}

oss_command ap 0x80 {
#
# Access point configuration
#
	# Node ID (setup Id)
	word	nodeid;
	# WOR setting for the interval (preamble length)
	word	worprl;
	# Number of packets to send as wake packets
	byte	nworp;
	# Packet retry count
	byte	norp;
}

#############################################################################
#############################################################################

oss_message status 0x02 {
#
# Status info
#
	lword	uptime;
	lword	seconds;
	lword	left;
	word 	battery;
	word	freemem;
	word	minmem;
	word	rate;
	byte	sset;
	# 21 bytes so far
	# sensor conf, all sensors, 17 nibbles, 9 bytes, total = 30 (39)
	blob	sstat;
}

oss_message report 0x03 {
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

oss_message ap 0x80 {
#
# To be extended later
#
	word	nodeid;
	word	worprl;
	byte	nworp;
	byte	norp;
}

##############################################################################
##############################################################################

proc parse_selector { } {

	return [oss_parse -skip -match {^-([[:alnum:]]+)} -return 2]
}

proc parse_value { sel min max } {

	set val [oss_parse -skip -number -return 1]

	if { $val == "" } {
		error "$sel, illegal value"
	}

	if [catch { oss_valint $val $min $max } val] {
		error "$sel, illegal value, $val"
	}

	return $val
}

proc parse_check_empty { } {

	set cc [oss_parse -skip " \t," -match ".*" -return 1]
	if { $cc != "" } {
		error "superfluous arguments: $cc"
	}
}

proc parse_grain { sel } {

	variable SNAMES

	set val [oss_parse -skip -match {^[[:alpha:]]+} -return 1]

	if { $val == "" } {
		error "argument of -$sel illegal or missing"
	}

	if [catch { oss_keymatch $val $SNAMES(G) } k] {
		error "$val is not a legal option for -$sel"
	}

	set ix [lsearch -exact $SNAMES(G) $k]

	if { $ix < 0 } {
		# impossible
		error "impossible in parse_grain"
	}

	return $ix
}

proc parse_bool { sel } {

	set val [oss_parse -skip -match {^[[:alnum:]]+} -return 1]
oss_ttyout "VAL = $val\n"

	if [catch { expr { $val + 0 } } num] {
		set val [string tolower $val]
		if { $val == "y" || $val == "yes" } {
			return 1
		}
		if { $val == "n" || $val == "no" } {
			return 0
		}
oss_ttyout "VALAL: $val"
		error "illegal boolean value $val for -$sel"
	} elseif { $num != 0 } {
oss_ttyout "VALEX: $val"
		return 1
	}
oss_ttyout "VALZR: $val"

	return 0
}

proc parse_component { sel cmp } {

	set val [oss_parse -skip -match {^[[:alpha:]]+} -return 1]

	set nc [string length $val]
	set re 0

	for { set i 0 } { $i < $nc } { incr i } {
		set c [string index $val $i]
		set f [string first $c $cmp]
		if { $f < 0 } {
			error "illegal component '$c', options are '$cmp'"
		}
		set re [expr { $re | (1 << $f) }]
	}

	return $re
}

###############################################################################
# Commands ####################################################################
###############################################################################

variable CMDS

set CMDS(configure)	"parse_cmd_configure"
set CMDS(radio)		"parse_cmd_radio"
set CMDS(on)		"parse_cmd_on"
set CMDS(off)		"parse_cmd_off"
set CMDS(sample)	"parse_cmd_sample"
set CMDS(stop)		"parse_cmd_stop"
set CMDS(status)	"parse_cmd_status"
set CMDS(ap)		"parse_cmd_ap"

variable LASTCMD	""

proc parse_cmd { line } {

	variable CMDS
	variable LASTCMD

	set cc [oss_parse -start $line -skip -return 0]
	if { $cc == "" || $cc == "#" } {
		# empty or comment
		return
	}

	if { $cc == "!" } {
		if { $LASTCMD == "" } {
			error "no previous command"
		}
		parse_cmd $LASTCMD
		return
	}

	if { $cc == ":" } {
		# a script in the remainder of the line
		set cc [string trim \
			[oss_parse -match "." -match ".*" -return 1]]
		set res [oss_evalscript $cc]
		oss_ttyout $res
		return
	}

	set cmd [oss_parse -subst -skip -match {^[[:alpha:]_][[:alnum:]_]*} \
		-return 2]

	if { $cmd == "" } {
		# no keyword
		error "illegal command syntax, must start with a keyword"
	}

	set cc [oss_keymatch $cmd [array names CMDS]]
	oss_parse -skip

	oss_ttyout $line

	$CMDS($cc)

	set LASTCMD $line
}

##############################################################################

proc parse_cmd_configure { } {

	variable SNAMES

	# initialize the blob
	set bb ""

	while 1 {
		# expect a keyword identifying a sensor
		set what [oss_parse -skip -match {^[[:alpha:]]+} -return 1]
		if { $what == "" } {
			# this is the end
			break
		}
		set k [oss_keymatch $what $SNAMES(S)]
		# handled already?
		if [info exists handled($k)] {
			error "duplicate $k"
		}
		set handled($k) ""
		set rs [do_config $k]
		set le [llength $rs]
		# sensor number
		set ix [lsearch -exact $SNAMES(S) $k]
		if { $ix < 0 } {
			# this cannot happen
			error "impossible: $k not found in \"$SNAMES(S)\""
		}
		if { $le > 0 } {
			# make it length - 1
			incr le -1
			if { $le > 15 } {
				# a sanity check
				error "too many settings for $k"
			}
			lappend bb [expr { ($ix << 4) | $le }]
			set bb [concat $bb $rs]
		}
	}

	parse_check_empty

	# issue the command
	if { $bb == "" } {
		error "nothing to configure"
	}

	oss_issuecommand 0x01 [oss_setvalues [list $bb] "config"]
}

proc do_config { sen } {
#
# Generate the blob sequence for the given sensor
#
	variable CPARAMS
	variable COMPS

	lassign $CPARAMS($sen) klist modes

	# initialize the blob portion
	set bb ""

	while 1 {

		set tp [parse_selector]

		if { $tp == "" } {
			break
		}

		set k [oss_keymatch $tp $klist]

		if [info exists handled($k)] {
			error "duplicate -$k for $sen"
		}

		set handled($k) ""

		set ix [lsearch -exact $klist $k]
		if { $ix < 0 } {
			error "impossible: $k not found in do_config"
		}
		set m [lindex $modes $ix]

		if { $m == "b" } {
			# boolean
			set v [parse_bool $k]
		} elseif { $m == "g" } {
			set v [parse_grain $k]
		} else {
			# components
			set v [parse_component $k $COMPS($sen)]
		}

		lappend bb [expr { ($ix << 4) | $v } ]
		continue

	}

	if { $bb == "" } {
		error "null configuration for $sen"
	}

	return $bb
}

proc parse_cmd_radio { } {

	set klist { "on" "wor" "off" "hibernate" }

	# 0 means no change
	set ofd 0
	set win 0

	set tp [parse_selector]

	if { $tp == "" } {
		error "argument missing"
	}

	set k [oss_keymatch $tp $klist]

	if { $k == "on" } {
		set opt 2
	} elseif { $k == "off" } {
		set opt 0
	} elseif { $k == "hibernate" } {
		set opt 3
	} else {
		# WOR
		set opt 1
		set ofd [oss_parse -skip -number -return 1]
		if { $ofd != "" } {
			if { $tp < 128 || $tp > 65535 } {
				error "off delay must be between 128 and 65535"
			}
			set win [oss_parse -skip -number -return 1]
			if { $win != "" } {
				if { $win < 128 || $win > 4096 } {
					error "WOR interval must be between \
						128 and 4096"
				}
			}
		}
	}

	parse_check_empty

	oss_issuecommand 0x02 [oss_setvalues [list $ofd $win $opt] "radio"]
}

proc parse_cmd_status { } {

	# no arguments
	parse_check_empty

	# the argument is ignored
	oss_issuecommand 0x03 [oss_setvalues [list 3] "status"]
}

proc parse_cmd_on { } {

	parse_cmd_on_off 0x80

}

proc parse_cmd_off { } {

	parse_cmd_on_off 0x00
}

proc parse_cmd_on_off { opt } {

	variable SNAMES

	while 1 {

		set sen [oss_parse -skip -match {^[[:alpha:]]+} -return 1]
		if { $sen == "" } {
			break
		}

		set k [oss_keymatch $sen $SNAMES(S)]

		if [info exists handled($k)] {
			error "duplicate $k"
		}

		set handled($k) ""

		set sn [lsearch -exact $SNAMES(S) $k]
		if { $sn < 0 } {
			error "impossible in parse_cmd_on"
		}

		set opt [expr { $opt | (1 << $sn) }]

	}

	parse_check_empty

	oss_issuecommand 0x04 [oss_setvalues [list $opt] "onoff"]
}

proc parse_cmd_sample { } {
#
# Start sampling
#
	set frq 0
	set cnt 1

	set klist { "frequency" "count" }
	# current time
	set ctime [clock seconds]

	while 1 {

		set tp [parse_selector]

		if { $tp == "" } {
			break
		}

		set k [oss_keymatch $tp $klist]

		if [info exists handled($k)] {
			error "duplicate -$k"
		}

		set handled($k) ""

		if { $k == "frequency" } {
			set frq [parse_value "-frequency" 1 256]
			continue
		}

		if { $k == "count" } {
			set cnt [oss_parse -skip -number -return 1]
			if { $cnt == "" } {
				# try infinite
				set cnt [string tolower [oss_parse \
					-match {^[[:alpha:]]+} -return 0]]
				if { $cnt == "" || [string first $cnt \
					"infinite"] != 0 } {
					error "illegal -count, must be a\
						number or \"infinite\""
				}
				set cnt [expr 0x0ffffffff]
			} elseif { $cnt < 0 } {
				error "-count cannot be negative"
			}

			continue
		}
	}

	parse_check_empty

	oss_issuecommand 0x05 [oss_setvalues [list $ctime $cnt $frq] \
		"sample"]
}

proc parse_cmd_stop { } {

	# no arguments
	parse_check_empty

	oss_issuecommand 0x06 [oss_setvalues [list 0] "stop"]
}

proc parse_cmd_ap { } {

	# unused
	set nodeid 0xFFFF
	set worprl 0xFFFF
	set worp 0xFF
	set norp 0xFF

	while 1 {

		set tp [parse_selector]
		if { $tp == "" } {
			break
		}

		set k [oss_keymatch $tp { "node" "wake" "retries" "preamble" }]

		if [info exists handled($k)] {
			error "duplicate -$k"
		}

		set handled($k) ""

		if { $k == "node" } {
			set nodeid [parse_value "-node" 1 65534]
			continue
		}

		if { $k == "wake" } {
			set worp [parse_value "-wake" 0 3]
			continue
		}

		if { $k == "preamble" } {
			set worprl [parse_value "preamble" 256 4096]
			continue
		}

		set norp [parse_value "retries" 0 7]
	}

	parse_check_empty

	oss_issuecommand 0x80 \
		[oss_setvalues [list $nodeid $worprl $worp $norp] "ap"]
}

###############################################################################
				
proc show_msg { code ref msg } {

	if { $code == 0 } {
		# ACK or NAK
		if { $ref != 0 } {
			variable ACKCODE
			binary scan $msg su msg
			if [info exists ACKCODE($msg)] {
				oss_ttyout "<$ref>: $ACKCODE($msg)"
			} else {
				oss_ttyout "<$ref>: response code $msg"
			}
		}
		return
	}

	set str [oss_getmsgstruct $code name]

	if { $str == "" } {
		# no such mwessage, this will trigger default dump
		error "no support"
	}

	show_msg_$name $msg
}

proc get_rss { msg } {

	binary scan [string range $msg end end] cu rs

	return "RSS: $rs"
}

proc sectoh { se } {

	set ti ""
	if { $se >= 3600 } {
		set hm [expr { $se / 3600 }]
		set se [expr { $se - ($hm * 3600) }]
		lappend ti "${hm}h"
	}
	if { $se >= 60 } {
		set hm [expr { $se / 60 }]
		set se [expr { $se - ($hm * 60) }]
		lappend ti "${hm}m"
	}
	if { $se > 0 || $ti == "" } {
		lappend ti "${se}s"
	}
	return [join $ti ","]
}

proc sset_to_string { ss } {
#
# Sensor set to string
#
	variable SNAMES(A)

	set ns [string length $SNAMES(A)]
	set rs ""

	for { set i 0 } { i < $ns } { incr i } {
		if { [expr { $ss & (1 << $i) }] } {
			append rs [string index $SNAMES(A) $i]
		} else {
			append rs "."
		}
	}

	return $rs
}

proc coll_stat { lft rat } {

	if { $lft == 0 } {
		return "NO"
	}

	if { $lft == 0x0ffffffff } {
		set rs "FOREVER"
	} else {
		set rs "$lft samples left"
	}

	return "$rs \[rate: $rat\]"

}

proc read_nibbles { blb } {

	# initialize: at even nibble
	set nib 1
	set ble [llength $blb]
	yield
	while 1 {
		if $nib {
			# get next byte
			if { $ble == 0 } {
				# done
				return ""
			}
			set cby [lindex $blb 0]
			set blb [lrange $blb 1 end]
			incr ble -1
			# odd nible
			yield [expr { ($cby >> 4 ) & 0xf }]
			set nib 0
		} else {
			# odd nible
			yield [expr { $cby & 0xf }]
			set nib 1
		}
	}
}

proc sensor_names { act } {
#
# Names of active sensors
#
	variable SNAMES

	set i 1
	set s ""
	foreach sn $SNAMES(S) {
		if { [expr { $act & $i }] } {
			lappend s $sn
		}
		set i [expr { $i << 1 }]
	}

	if { $s == "" } {
		return "None"
	}

	return [join $s ", "]
}

proc show_msg_status { msg } {

	variable SNAMES
	variable CPARAMS
	variable COMPS

	lassign [oss_getvalues $msg "status"] upt sec lft bat frm mim rat sns \
		spa

	set res "Node status ([get_rss $msg]):\n"
	append res "  Uptime:      [sectoh $upt]\n"
	append res "  Time:        [clock format $sec]\n"
	append res "  Battery:     [sensor_to_voltage $bat]\n"
	append res "  Memory:      F: $frm M: $mim\n"
	append res "  Active:      [sensor_names $sns]\n"
	append res "  Collecting:  R: $rat L: $lft\n"

	# Now go through sensor configs, the length is in fact static, but we
	# use a blob for that

	set sn [llength $SNAMES(S)]

	# set the coroutine for reading nibbles from the blob
	coroutine read_next_param read_nibbles $spa

	set er 0
	for { set i 0 } { $i < $sn } { incr i } {
		set sname [lindex $SNAMES(S) $i]
		append res "\n  Sensor $sname:\n"
		lassign $CPARAMS($sname) parnames partypes
		set j 0
		foreach pname $parnames {
			set ptype [lindex $partypes $j]
			incr j
			set val [read_next_param]
			if { $val == "" } {
				# premature end
				set er 1
				break
			}
			if { $ptype == "g" } {
				# transform into a symbolic grade
				set gg [lindex $SNAMES(G) $val]
				if { $gg == "" } {
					set gg "illegal"
				}
			} elseif { $ptype == "b" } {
				if $val {
					set gg "YES"
				} else {
					set gg "NO"
				}
			} else {
				# components
				if [info exists COMPS($sname)] {
					set cs $COMPS($sname)
					set cl [string length $cs]
					set gg ""
					for { set k 0 } { $k < $cl } \
						{ incr k } {
						if { [expr { (1<<$k)&$val }] } {
							append gg [string \
								index $cs $k]
						} else {
							append gg "."
						}
					}
				} else {
					set gg [format %1x $val]
				}
			}
			append res "    [tab_string $pname 12] $gg\n"
		}
		if $er { break }
	}

	# delete the coroutine
	catch { rename read_next_param "" }

	if $er {
		append res "\n ... list truncated, data too short\n"
	}

	oss_ttyout $res
}

proc show_msg_report { msg } {

	lassign [oss_getvalues $msg "report"] sample layout data

	# The "layout" layout:
	#
	#	uuuuupplmhhiiiii
	#
	#	u - unused
	#	i - imu components mtcga (m == motion, other bits ignored)
	#	... and so on

	set res "$sample "

	# imu components
	set cmp [expr { $layout & 0x1f }]
	if $cmp {
		append res [show_report_imu data $cmp]
	}

	set cmp [expr { ($layout >> 5) & 0x3 }]
	if $cmp {
		append res [show_report_humid data $cmp]
	}

	if [expr { ($layout >> 7) & 0x1 }] {
		append res [show_report_mic data]
	}

	if [expr { ($layout >> 8) & 0x1 }] {
		append res [show_report_light data]
	}

	set cmp [expr { ($layout >> 9) & 0x3 }]
	if $cmp {
		append res [show_report_press data $cmp]
	}

	oss_ttyout $res
}

proc get_u16 { bb } {

	upvar $bb data

	if { [llength $data] < 2 } {
		error "blob too short"
	}

	set a [lindex $data 0]
	set b [lindex $data 1]

	set data [lrange $data 2 end]

	return [expr { ($b << 8) | $a }]
}

proc get_u32 { bb } {

	upvar $bb data

	if { [llength $data] < 4 } {
		error "blob too short"
	}

	set a [lindex $data 0]
	set b [lindex $data 1]
	set c [lindex $data 2]
	set d [lindex $data 3]

	set data [lrange $data 4 end]

	return [expr { ($d << 24) | ($c << 16) | ($b << 8) | $a }]
}

proc get_f16 { bb } {

	upvar $bb data

	set w [get_u16 data]

	if [expr { $w & 0x8000 }] {
		set w [expr { -65536 + $w }]
	}

	return [format %7.4f [expr { $w / 32768.0 }]]
}

proc get_t16 { bb } {
#
# FP scaled as temp/humid (divided by 100.0)
#
	upvar $bb data

	set w [get_u16 data]

	if [expr { $w & 0x8000 }] {
		set w [expr { -65536 + $w }]
	}

	return [format %7.2f [expr { $w / 100.0 }]]
}

proc get_t32 { bb } {
#
# FP scaled as temp (divided by 100.0)
#
	upvar $bb data

	set w [get_u32 data]

	if [expr { $w & 0x80000000 }] {
		set w [expr { -4294967296 + $w }]
	}

	return [format %7.2f [expr { $w / 100.0 }]]
}

proc get_i16 { bb } {

	upvar $bb data

	set w [get_u16 data]

	if [expr { $w & 0x8000 }] {
		set w [expr { -65536 + $w }]
	}

	return [format %6d $w]
}

proc get_n16 { bb } {

	upvar $bb data

	set w [get_u16 data]

	return [format %5u $w]
}

proc get_n32 { bb } {

	upvar $bb data

	set w [get_u32 data]

	return [format %11u $w]
}

proc get_e16 { bb } {
#
# Exp + manitissa (light)
#
	upvar $bb data

	set w [get_u16 data]

	set exp [expr { ($w >> 12) & 0xF }]
	set man [expr { ($w & 0x0FFF) * pow (2.0, $exp) }]

	return [format %8.2e $man]
}

proc tab_string { s n } {

	set l [expr { $n - [string length $s] } ]
	if { $l > 0 } {
		append s [string repeat " " $l]
	}

	return $s
}

proc show_report_imu { d cmp } {

	upvar $d data

	set res " IMU:"

	if [expr { $cmp & 0x10 }] {
		# this indicates a motion event whose format is three
		# coordinates + count
		set x [get_f16 data]
		set y [get_f16 data]
		set z [get_f16 data]
		set c [get_n16 data]
		append res " \[M $x $y $x $c\]"
		# no need to look at the remaining bits
		return $res
	}

	if [expr { $cmp & 0x01 }] {
		# accel present
		set x [get_f16 data]
		set y [get_f16 data]
		set z [get_f16 data]
		append res " \[A $x $y $z\]"
	}

	if [expr { $cmp & 0x02 }] {
		# gyro present
		set x [get_f16 data]
		set y [get_f16 data]
		set z [get_f16 data]
		append res " \[G $x $y $z\]"
	}

	if [expr { $cmp & 0x04 }] {
		# compass present
		set x [get_f16 data]
		set y [get_f16 data]
		set z [get_f16 data]
		append res " \[C $x $y $z\]"
	}

	if [expr { $cmp & 0x08 }] {
		# temp present, show it as an integer
		append res " \[T [get_i16 data]\]"
	}

	return $res
}

proc show_report_humid { d cmp } {

	upvar $d data

	set res " HUM:"

	if [expr { $cmp & 0x01 }] {
		# humidity present
		append res " \[H [get_t16 data]\]"
	}

	if [expr { $cmp & 0x02 }] {
		# temp present
		append res " \[T [get_t16 data]\]"
	}

	return $res
}

proc show_report_mic { d } {

	upvar $d data

	set res " MIC:"

	set s [get_n32 data]
	set a [get_n32 data]

	append res " \[SA $s $a\]"

	return $res
}

proc show_report_light { d } {

	upvar $d data

	set res " LIG:"

	# just one word
	append res " \[[get_e16 data]\]"

	return $res
}

proc show_report_press { d cmp } {

	upvar $d data

	set res " PRE:"

	if [expr { $cmp & 0x01 }] {
		# pressure present
		append res " \[P [get_n32 data]\]"
	}

	if [expr { $cmp & 0x02 }] {
		# temp present
		append res " \[T [get_t32 data]\]"
	}

	return $res
}

proc show_msg_ap { msg } {

	lassign [oss_getvalues $msg "ap"] nodeid worprl nworp norp

	set res "AP status:\n"
	append res "  Node Id (-node):                   $nodeid\n"
	append res "  WOR preamble length (-preamble):   $worprl\n"
	append res "  Copies of WOR packet (-wake):      $nworp\n"
	append res "  Copies of CMD packet (-retries):   $norp\n"

	oss_ttyout $res
}

proc start_up { } {
#
# Send a dummy ap message to initialize reference counter
#
	# oss_dump -incoming -outgoing
	oss_issuecommand 0x80 [oss_setvalues [list 0 0 0 0] "ap"]
}
