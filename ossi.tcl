###############################################################################
# Configuration parameters ####################################################
###############################################################################
#
# Sensor names. This assigns numerical identifiers to the sensors in the listed
# order, starting from zero.
#
variable SNAMES
#
set SNAMES(S) 		{ "imu" "microphone" "pressure" "humidity" "light" }
# Single-letter abbreviations
set SNAMES(A)		"imphl"
#
# Discrete gradation for those parameters that fit the "g" class. Actual 
# numerical parameters will be mapped into this scale by the node. These
# are synonymes for 0-7.
#
set SNAMES(G) 		{ "tiny" "low" "small" "medium" "big" "high" "huge" 
				"extreme" }

#
# Components by sensor. Only for those sensors that have components. Map to
# bits (a = 1, g = 2, c = 4, t = 8).
#
variable COMPS
#
set COMPS(imu) 		"agct"
set COMPS(humidity)	"ht"
set COMPS(pressure)	"pt"

#
# Specific parameters by sensor: b = yes/no, g = graded, string = component
# selection.
#
variable CPARAMS
#
set CPARAMS(imu)	{
				{ "events" "threshold" "rate" "accuracy"
				  "bandwidth" "components" "report" "datarate" }
				{ "nmd" g g g g "agct" b g }
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
# Convert battery sensor reading to battery voltage
#
proc sensor_to_voltage { val } {

	# integer volts
	set g [expr { ($val >> 5) & 0xf }]
	set f [expr { double($val & 0x1f) / 32.0 }]
	return [format "%4.2fV" [expr { double($g) + $f }]]
}

###############################################################################
###############################################################################

# These should match the definitions in rf.h

variable ACKCODE

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

#############################################################################
#############################################################################

# (TODO) check if the speed can be increased

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

oss_command wake 0x02 {
#
# Query or set the device status
#
	byte	dummy;
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
	word	freemem;
	word	minmem;
	word	rate;
	byte 	battery;
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
	byte	voltage;
}

# streaming blocks interpreted separately (in a non-standard way)

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

proc parse_empty { } {

	set cc [oss_parse -skip " \t," -match ".*" -return 1]
	if { $cc != "" } {
		error "superfluous arguments: $cc"
	}
}

proc parse_grade { sel } {

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
		error "impossible in parse_grade"
	}

	return $ix
}

proc parse_bool { sel } {

	set val [oss_parse -skip -match {^[[:alnum:]]+} -return 1]

	if [catch { expr { $val + 0 } } num] {
		set val [string tolower $val]
		if { $val == "y" || $val == "yes" } {
			return 1
		}
		if { $val == "n" || $val == "no" } {
			return 0
		}
		error "illegal boolean value $val for -$sel"
	} elseif { $num != 0 } {
		return 1
	}

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
set CMDS(wake)		"parse_cmd_wake"
set CMDS(on)		"parse_cmd_on"
set CMDS(off)		"parse_cmd_off"
set CMDS(sample)	"parse_cmd_sample"
set CMDS(stream)	"parse_cmd_stream"
set CMDS(stop)		"parse_cmd_stop"
set CMDS(status)	"parse_cmd_status"
set CMDS(ap)		"parse_cmd_ap"
set CMDS(mreg)		"parse_cmd_mreg"

variable LASTCMD	""

variable CTiming	0

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

	oss_out $line

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

		if { $rs == "" } {
			error "null configuration for $k"
		}

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
			lappend bb [expr { ($ix << 4) | $le }]
			set bb [concat $bb $rs]
		}
	}

	parse_empty

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
			set v [parse_grade $k]
		} else {
			# components
			set v [parse_component $k $m]
		}

		lappend bb [expr { ($ix << 4) | $v } ]
		continue

	}

	if { [llength $bb] > 16 } {
		error "too many settings for $sen"
	}

	return $bb
}

proc parse_cmd_wake { } {

	parse_empty

	oss_issuecommand 0x02 [oss_setvalues [list 0] "wake"]
}

proc parse_cmd_status { } {

	# no arguments
	parse_empty

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

	parse_empty

	oss_issuecommand 0x04 [oss_setvalues [list $opt] "onoff"]
}

proc parse_cmd_sample { } {
#
# Start sampling or streaming
#
	set frq 0
	set cnt 1

	set klist { "frequency" }

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
			set frq [parse_value "-frequency" 1 [expr { 256 * 60 }]]
			continue
		}
	}

	parse_empty

	oss_issuecommand 0x05 [oss_setvalues [list $frq] "sample"]
}

proc parse_cmd_stream { } {

	set rs [do_config "imu"]
	set le [llength $rs]
	if { $le } {
		incr le -1
		# IMU is number zero, so the config sequence looks right
		set bb [concat [list $le] $rs]
	} else {
		set bb ""
	}

	oss_issuecommand 0x06 [oss_setvalues [list $bb] "stream"]
}

proc parse_cmd_stop { } {

	# no arguments
	parse_empty

	oss_issuecommand 0x07 [oss_setvalues [list 0] "stop"]
}

proc parse_cmd_mreg { } {

	set tp [parse_selector]

	if { $tp == "" } {
		error "-read or -write"
	}

	set k [oss_keymatch $tp { "read" "write" }]

	set reg [parse_value "reg number" 0 255]

	if { $k == "read" } {
		# reg number + optional length
		if [catch { parse_value "size" 1 32 } siz] {
			set siz 1
		}
		oss_issuecommand 0x09 [oss_setvalues [list 0 $reg $siz] "mreg"]
		return
	}

	# write
	set val [parse_value "value" 0 255]
	oss_issuecommand 0x09 [oss_setvalues [list 1 $reg $val] "mreg"]
}

proc parse_cmd_ap { } {

	# unused
	set nodeid 0xFFFF
	set loss 0xFFFF
	set nretr 0xFF

	while 1 {

		set tp [parse_selector]
		if { $tp == "" } {
			break
		}

		set k [oss_keymatch $tp { "node" "retries" "loss" }]

		if [info exists handled($k)] {
			error "duplicate -$k"
		}

		set handled($k) ""

		if { $k == "node" } {
			set nodeid [parse_value "-node" 1 65534]
			continue
		}

		if { $k == "loss" } {
			set loss [parse_value "loss" 0 1024]
			continue
		}

		set nretr [parse_value "retries" 0 7]
	}

	parse_empty

	oss_issuecommand 0x08 \
		[oss_setvalues [list $nodeid $nretr $loss] "ap"]
}

###############################################################################
				
proc show_msg { code ref msg } {

	if { $code == 0 } {
		# ACK or NAK
		if { $ref != 0 } {
			variable ACKCODE
			binary scan $msg su msg
			if [info exists ACKCODE($msg)] {
				oss_out "<$ref>: $ACKCODE($msg)"
			} else {
				oss_out "<$ref>: response code $msg"
			}
		}
		return
	}

	if { $code == 128 } {
		show_sblock $ref $msg
		return
	}

	if { $code == 129 } {
		show_eot $ref $msg
		return
	}

	# a standard message

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

	lassign [oss_getvalues $msg "status"] upt tak frm mim rat bat sns sta \
		spa

	if { $sta == 0 } {
		set sta "IDLE"
	} elseif { $sta == 1 } {
		set sta "SAMPLING"
	} else {
		set sta "STREAMING"
	}

	set res "Node status ([get_rss $msg]):\n"
	append res "  Uptime:      [sectoh $upt]\n"
	append res "  Battery:     [sensor_to_voltage $bat]\n"
	append res "  Memory:      F: $frm M: $mim\n"
	append res "  Status:      $sta\n"
	append res "  Active:      [sensor_names $sns]\n"
	append res "  Taken:       $tak @ $rat\n"

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
				set cl [string length $ptype]
				set gg ""
				for { set k 0 } { $k < $cl } { incr k } {
					if { [expr { (1 << $k) & $val }] } {
						append gg \
						    [string index $ptype $k]
					} else {
						append gg "."
					}
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

	oss_out $res
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

	if [expr { ($layout >> 7) & 0x1 }] {
		append res [show_report_mic data]
	}

	set cmp [expr { ($layout >> 9) & 0x3 }]
	if $cmp {
		append res [show_report_press data $cmp]
	}

	set cmp [expr { ($layout >> 5) & 0x3 }]
	if $cmp {
		append res [show_report_humid data $cmp]
	}

	if [expr { ($layout >> 8) & 0x1 }] {
		append res [show_report_light data]
	}

	oss_out $res
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

proc to_f16 { w } {
#
# 16-bit to float
#
	if [expr { $w & 0x8000 }] {
		set w [expr { -65536 + $w }]
	}

	return [format %7.4f [expr { $w / 32768.0 }]]
}

proc get_f16 { bb } {

	upvar $bb data

	return [to_f16 [get_u16 data]]
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
		# this indicates a motion event whose format is simple
		# coordinates + count
		set c [get_n16 data]
		append res " \[M $c\]"
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

proc show_msg_motion { msg } {

	lassign [oss_getvalues $msg "motion"] evt acc
	lassign $acc x y z

	oss_out "Motion: ([format %5u $evt])\
		 \[A [to_f16 $x] [to_f16 $y] [to_f16 $z]\]"
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

	lassign [oss_getvalues $msg "ap"] nodeid nretr loss

	set res "AP status:\n"
	append res "  Node Id (-node):                   $nodeid\n"
	append res "  Copies of cmd packet (-retries):   $nretr\n"
	append res "  Loss (packets per 1024):           $loss\n"

	oss_out $res
}

proc show_msg_mreg { msg } {

	lassign [oss_getvalues $msg "mreg"] data

	set res ""

	foreach b $data {
		append res [format " %02x" $b]
	}

	oss_out $res
}

proc start_up { } {
#
# Send a dummy ap message to initialize the reference counter
#
	variable CTiming

	# oss_dump -incoming -outgoing
	oss_issuecommand 0x08 [oss_setvalues [list 0 0 0 0] "ap"]

	set CTiming [clock milliseconds]
}

proc timing_out { } {

	variable CTiming

	set t $CTiming
	set CTiming [clock milliseconds]
	set s [expr { $CTiming / 1000 }]
	set m [expr { $CTiming - ( $s * 1000 ) }]
	set r [clock format $s -format "%H:%M:%S"]
	append r [string range [format "%4.3f" [expr { $m / 1000.0 }]] 1 end]
	append r " ("
	append r [format "%4.3f" [expr { ($CTiming - $t) / 1000.0 }]]
	append r ") -> "
	return $r
}

proc oss_out { msg } {

	oss_ttyout "[timing_out]$msg"
}

###############################################################################
# An add on for showing streaming accel data
###############################################################################

proc show_sblock { ref dat } {

	set dat [lindex [oss_getvalues $dat "sblock"] 0]

	set bn $ref

	set sh 8
	set fm ""
	for { set i 0 } { $i < 12 } { incr i } {
		set ci [lindex $dat $i]
		# turn them into 16-bit floats
		append fm [to_f16 [expr { ($ci >> 16) & 0xffc0 }]]
		append fm " "
		append fm [to_f16 [expr { ($ci >>  6) & 0xffc0 }]]
		append fm " "
		append fm [to_f16 [expr { ($ci <<  4) & 0xffc0 }]]
		append fm "\n"
		set b [expr {  $ci        & 0x0003 }]
		set bn [expr { $bn |   ($b << $sh) }]
		incr sh 2
	}

	oss_out "B: [format %10u $bn]\n$fm"
}

proc show_eot { ref dat } {

	lassign [oss_getvalues $dat "etrain"] last offset sclock drop bat

	oss_out "E: [format %10u $last] [format %5u $offset]\
		 [format %5u $sclock] [format %3u $drop]\
			[sensor_to_voltage $bat]"
}
