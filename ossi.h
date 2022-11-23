#ifndef  __ossi_h_pg__
#define  __ossi_h_pg__
#include "sysio.h"
// =================================================================
// Generated automatically, do not edit (unless you really want to)!
// =================================================================

#define	OSS_PRAXIS_ID		65570
#define	OSS_UART_RATE		230400
#define	OSS_PACKET_LENGTH	56

typedef	struct {
	word size;
	byte content [];
} blob;

typedef	struct {
	byte code, ref;
} oss_hdr_t;

// ==================
// Command structures
// ==================

#define	command_config_code	1
typedef struct {
	blob	confdata;
} command_config_t;

#define	command_setp_code	10
typedef struct {
	blob	params;
} command_setp_t;

#define	command_wake_code	2
typedef struct {
	byte	dummy;
} command_wake_t;

#define	command_status_code	3
typedef struct {
	byte	dummy;
} command_status_t;

#define	command_onoff_code	4
typedef struct {
	byte	which;
} command_onoff_t;

#define	command_sample_code	5
typedef struct {
	word	spm;
} command_sample_t;

#define	command_stream_code	6
typedef struct {
	blob	confdata;
} command_stream_t;

#define	command_stop_code	7
typedef struct {
	byte	dummy;
} command_stop_t;

#define	command_ap_code	8
typedef struct {
	word	nodeid;
	byte	nretr;
	byte	halt;
	word	loss;
} command_ap_t;

#define	command_mreg_code	9
typedef struct {
	byte	what;
	byte	regn;
	byte	value;
} command_mreg_t;

// ==================
// Message structures
// ==================

#define	message_config_code	1
typedef struct {
	blob	confdata;
} message_config_t;

#define	message_setp_code	10
typedef struct {
	blob	params;
} message_setp_t;

#define	message_sblock_code	128
typedef struct {
	lword	data [12];
} message_sblock_t;

#define	message_etrain_code	129
typedef struct {
	lword	last;
	word	offset;
	byte	voltage;
	byte	flags;
} message_etrain_t;

#define	message_status_code	3
typedef struct {
	lword	uptime;
	lword	taken;
	lword	fover;
	lword	mfail;
	lword	qdrop;
	lword	ploss;
	word	freemem;
	word	minmem;
	word	rate;
	byte	battery;
	byte	sset;
	byte	status;
} message_status_t;

#define	message_report_code	5
typedef struct {
	word	sample;
	word	layout;
	blob	data;
} message_report_t;

#define	message_motion_code	6
typedef struct {
	word	events;
	word	accel [3];
} message_motion_t;

#define	message_ap_code	8
typedef struct {
	word	nodeid;
	byte	nretr;
	word	loss;
} message_ap_t;

#define	message_mreg_code	9
typedef struct {
	blob	data;
} message_mreg_t;


// ===================================
// End of automatically generated code 
// ===================================
#endif
