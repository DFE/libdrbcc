/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/*
 * Copyright (C) 2013 DResearch Fahrzeugelektronik GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <libgen.h>
#include <termios.h>
#include <unistd.h>
#define _XOPEN_SOURCE /* glibc2 needs this */
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <stdint.h>
#include <pthread.h>
#include <malloc.h>
#include <math.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <drbcc.h>
#include <drbcc_files.h>

#ifdef HAVE_LIBDRTRACE
#include <drhiptrace.h>
#else
#endif

#include "trace.h"
#include "drbcc_sema.h"

#if USE_OPEN_BINARY
#define OX_BINARY O_BINARY
#else
#define OX_BINARY 0
#endif

#define UNUSED(val) (void)(val)

#ifdef HAVE_LIBDRTRACE
DRHIP_TRACE_UNIT(drbcctrace);
#endif

typedef enum {
	cmdid_sync,
	cmdid_proto,
	cmdid_part,
	cmdid_getrtc,
	cmdid_setrtc,
	cmdid_setgpo,
	cmdid_setled,
	cmdid_getstatus,
	cmdid_flashid,
	cmdid_wflash,
	cmdid_rflash,
	cmdid_eflash,
	cmdid_gfile,
	cmdid_gfiletype,
	cmdid_pfile,
	cmdid_pfiletype,
	cmdid_dfile,
	cmdid_dfiletype,
	cmdid_blupl,
	cmdid_fwupl,
	cmdid_blupd,
	cmdid_fwinv,
	cmdid_restart,
	cmdid_getlog,
	cmdid_putlog,
	cmdid_getpos,
	cmdid_hdeject,
	cmdid_hdonoff,
	cmdid_gpipower,
	cmdid_shutdown,
	cmdid_heartbeat,
	cmdid_wait,
	cmdid_exit,
	cmdid_help,
	cmdid_getiddata,
	cmdid_clearlog,
	cmdid_debugset,
	cmdid_debugget,

	cmdid_unknown,
} cmdid_t;

typedef struct {
	cmdid_t id;
	char* cmdtext;
	unsigned matchlen;
	char* helptext;
} cmd_t;

const cmd_t s_cmds[] =
{
	{ cmdid_sync,		"sync",					sizeof("sy")-1,			"send SYNC message" },
	{ cmdid_proto,		"proto",				sizeof("pr")-1,			"request PROTOCOL info" },
	{ cmdid_part,		"part",					sizeof("pa")-1,			"request PARTITION TABLE info" },
	{ cmdid_getrtc,		"getrtc",				sizeof("getrtc")-1,		"request RTC info" },
	{ cmdid_setrtc,		"setrtc [T]",			sizeof("setrtc")-1,		"set RTC date/time to T (format YYYY-MM-DD hh:mm:ss) or current time" },
	{ cmdid_setgpo,		"setgpo N,S",			sizeof("setgpo")-1,		"set GPO N (1-4) to state S (0|1)" },
	{ cmdid_setled,		"setled N,C[,L,O,P]",	sizeof("setl")-1,		"set LED N (1-4) to color C (0|1|2|3), optional flashing with on-time L, off-time O and phase shift P (all in 1/20s)" },
	{ cmdid_getstatus,	"getstatus",			sizeof("gets")-1,		"get status message" },
	{ cmdid_getiddata,	"getid",				sizeof("getid")-1,		"get id data" },
	{ cmdid_flashid,	"flashid",				sizeof("fla")-1,		"request FLASHID info" },
	{ cmdid_rflash,		"rflash A,L[,F]",		sizeof("rfl")-1,		"read L bytes from flash address A into file F or hexdump to stdout" },
	{ cmdid_wflash,		"wflash A,L,F",			sizeof("wfl")-1,		"write L bytes from file F into flash address A" },
	{ cmdid_eflash,		"eflash N",				sizeof("efl")-1,		"erase 4k flash block N (range depends on flash type)" },
	{ cmdid_gfile,		"getfile E,F",			sizeof("getfi")-1,		"write data from index E to local file F" },
	{ cmdid_gfiletype,	"gfiletype X,F",		sizeof("gfilet")-1,		"write data from file X(0xTI, type T with sub-number I) to local file F" },
	{ cmdid_pfile,		"putfile I,T,F",		sizeof("putfi")-1,		"write data from local file F as type T with sub-number I" },
	{ cmdid_pfiletype,	"pfiletype X,F",		sizeof("pfilet")-1,		"write data from local file F as file X(0xTI, type T with sub-number I)" },
	{ cmdid_dfile,		"delfile E",			sizeof("delfi")-1,		"delete file at index E" },
	{ cmdid_dfiletype,	"dfiletype X",			sizeof("dfilet")-1,		"delete file X(0xTI, type T with sub-number I)" },
	{ cmdid_blupl,		"blupload F",			sizeof("blupl")-1,		"upload new bootloader from file F" },
	{ cmdid_fwupl,		"fwupload F",			sizeof("fwupl")-1,		"upload new firmware from file F" },
	{ cmdid_blupd,		"blupdate",				sizeof("blupd")-1,		"update bootloader" },
	{ cmdid_fwinv,		"fwinv",				sizeof("fwi")-1,		"invalidate firmware" },
	{ cmdid_restart,	"restart",				sizeof("re")-1,			"restart board controller when host power is off" },
	{ cmdid_getlog,		"getlog [[I][,R[,F]]]",	sizeof("getl")-1,		"get flash log\n"
									"\t\t\tI>=0: from log entry I to last\n"
									"\t\t\t I<0: last I entries\n"
									"\t\t\tno I: all\n"
									"\t\t\tR (0|1) show raw data (default 0), F file or dump to stdout" },
	{ cmdid_putlog,  "putlog [L]",				sizeof("putl")-1,		"generate a test flash log entry with L bytes (1-128, default 5)" },
	{ cmdid_getpos,  "getpos",					sizeof("getp")-1,		"get flash log position" },
	{ cmdid_clearlog,"clearlog",				sizeof("clearlog")-1,	"clear flash log" },
	{ cmdid_hdeject, "hdeject",					sizeof("hde")-1,		"eject hard disc" },
	{ cmdid_hdonoff, "hdpower I",				sizeof("hdp")-1,		"switch hard disc power I (0|1)" },
	{ cmdid_gpipower,"gpipower I",				sizeof("gpow")-1,		"switch GPI power I (0|1)" },
	{ cmdid_wait,    "wait N",					sizeof("wa")-1,			"wait N milliseconds before sending next command" },
	{ cmdid_shutdown,"shutdown N",				sizeof("shut")-1,		"shutdown with N (0-65535) seconds timeout for power off" },
	{ cmdid_heartbeat,"heartbeat N",			sizeof("heart")-1,		"heartbeat with N (0-65535) seconds timeout" },
	{ cmdid_exit,    "exit",					sizeof("ex")-1,			"stop sending commands and exit (same as 'quit')" },
	{ cmdid_exit,    "quit",					sizeof("q")-1,			"stop sending commands and exit (same as 'exit')" },
	{ cmdid_help,    "help",					sizeof("h")-1,			"print command help" },

	{ cmdid_debugset, "debugset A[,DATA]",		sizeof("debugset")-1,     "send debug set message with address A and optional DATA (as hex string, e.g. 13,050008000005)\n"
									"\t\t\tA=3 : (1/0) temperature test mode active/inactive 1 DATA byte required\n"
									"\t\t\tA=4 : temperature value in temp test mode as int8, 1 DATA byte required\n"
									"\t\t\tA=5 : (1/0) reset active/inactive 1 DATA byte required\n"
									"\t\t\tA=9 : write a log entry with all measured voltages\n"
									"\t\t\tA=10: GPI/SPI debounce time in milliseconds 1 DATA byte required\n"
									"\t\t\tA=13: acceleration test: 2 DATA byte per axis x, y, z required, lowbyte first\n"
									"\t\t\t\t value 1 is equivalent to 3,90625mg(1000/256), value 0xffff is equivalent to -3,90625mg\n"
									"\t\t\tA=16: GPI/SFI override: 3 DATA bytes required: 1st: 1/0 override active/inactive, 2nd: GPI override, 3rd: SFI override\n"
									"\t\t\tA=256: temp limit power low as int8, 1 DATA byte required\n"
									"\t\t\tA=257: temp limit power high as int8, 1 DATA byte required\n"
									"\t\t\tA=258: temp limit reset as int8, 1 DATA byte required\n"
									"\t\t\tA=259: temp limit hard high as int8, 1 DATA byte required\n"
									"\t\t\tA=261: acceleration threshold high: (g-value * 256)^2, 4 DATA byte required, hibyte first" },
	{ cmdid_debugget, "debugget A",				sizeof("debugset")-1,			"send debug get message with address A\n"
									"\t\t\tA=0 :  4 x 2 byte counter (hi lo): extuart dropped chars + crc errors, then same for oxeuart\n"
									"\t\t\tA=1 :  current value and limit of heartbeat timer in seconds, 2 x 2 byte, hibyte first\n"
									"\t\t\tA=10:  GPI/SPI debounce time in milliseconds\n"
									"\t\t\tA=256: temp limit power low as int8\n"
									"\t\t\tA=257: temp limit power high as int8\n"
									"\t\t\tA=258: temp limit reset as int8\n"
									"\t\t\tA=259: temp limit hard high as int8\n"
									"\t\t\tA=261: acceleration threshold high: (g-value * 256)^2, hibyte first" },

	{ cmdid_unknown, "unknown", 0, "" } // needs to be the last in this list!
};

static const char *s_progname;
static char s_cmd_line[1024] = "";
static char s_dev[FILENAME_MAX] = "/dev/ttyS0";
static DRBCC_BR_t s_speed = DRBCC_BR_921600;
static DRBCC_SESSION_t session;
static int s_running = 0;

typedef struct
{
	char flashread_filename[FILENAME_MAX];
	long readoffset;
	struct timeval tm_flashread_request;
	int raw; // 0=no additional raw output, 1=additional raw output
} context_t;
static context_t s_context;

typedef struct
{
	int running;
	unsigned int session_count;
	DRBCC_HANDLE_t h;
	unsigned int line_valid;
	const char *current_line;
	DRBCC_sema_t sema;
} DRBCC_thread_context_t;
static DRBCC_thread_context_t s_drbcc_thread_context = { 0, 0, 0, 0, 0, 0 };

static tracelevel_t s_tracelevel = 0;
 
tracelevel_t tracelevel()
{
   return s_tracelevel;
}
 
void set_tracelevel(tracelevel_t t)
{
   s_tracelevel = t;
}

static void session_start(DRBCC_thread_context_t *drbcc_thread)
{
	drbcc_thread->session_count++;
	TRACE(TL_DEBUG, "session_start: %d", drbcc_thread->session_count);
}

static void session_stop(DRBCC_thread_context_t *drbcc_thread)
{
	if(0 != drbcc_thread->session_count)
	{
		drbcc_thread->session_count--;
		drbcc_sema_release(drbcc_thread->sema);
	}
	TRACE(TL_DEBUG, "session_stop: %d", drbcc_thread->session_count);
}

static unsigned int session_active(DRBCC_thread_context_t *drbcc_thread)
{
	return drbcc_thread->session_count;
}
static unsigned int speed_2_uint(DRBCC_BR_t speed)
{
	switch(speed)
	{
	case DRBCC_BR_921600: return 921600;
	case DRBCC_BR_115200: return 115200;
	case DRBCC_BR_57600: return 57600;
	default: return 0;
	}
}
static void print_usage(FILE *out, int complete)
{
	unsigned i = 0;

	if(complete)
	{
		fprintf(out, "Usage: %s [options]\n", s_progname);
		fprintf(out, "Send/libdrbcc_receive messages to HydraIP board controller.\n\n");
		fprintf(out, "Options\n");
		fprintf(out, "  --version,       print version and exit successfully\n");
		fprintf(out, "  --help, -h       print this help and exit successfully\n");
		fprintf(out, "  --cmd=CMD        execute the CMD and exit\n");
		fprintf(out, "  --trace=LEVEL    set tracelevel (default: 1)\n");
		fprintf(out, "                   %u no debug info, just errors\n", TL_INFO);
		fprintf(out, "                   %u + global info and warnings\n", TL_DEBUG);
		fprintf(out, "                   %u + mainloop activities\n", TL_TRACE);
		fprintf(out, "  --libtrace=MASK  set tracemask for libdrbcc\n");
		fprintf(out, "                   0x%08X serial transport\n", (0xF<<(DRBCC_TR_TRANS*4)));
		fprintf(out, "                   0x%08X parameter parsing\n", (0xF<<(DRBCC_TR_PARAM*4)));
		fprintf(out, "                   0x%08X queue handling\n", (0xF<<(DRBCC_TR_QUEUE*4)));
		fprintf(out, "                   0x%08X message handling\n", (0xF<<(DRBCC_TR_MSGS*4)));
		fprintf(out, "  --dev=DEVICE[,SPEED]  character device of board controller\n");
		fprintf(out, "                   supported speeds: 57600, 115200, 921600(default %u)\n", speed_2_uint(s_speed));
		fprintf(out, "Commands accepted at stdin (new-line terminated):\n");
	}

	while(s_cmds[i].id != cmdid_unknown)
	{
		fprintf(out, "  %-14s  %s\n", s_cmds[i].cmdtext, s_cmds[i].helptext);
		i++;
	}

}

static void print_version(void)
{
	fprintf(stdout, "%s [$Id: 92834683f4db137bc3cfa2cb91c2faee8d4bcbc6 $]\n", s_progname);
}

static void sighandler(int signum)
{
	TRACE(TL_DEBUG, "got signal %d", signum);
	switch(signum)
	{
		case SIGTERM:
		case SIGINT:
			fclose(stdin);
			s_drbcc_thread_context.running = 0;
			drbcc_sema_release(s_drbcc_thread_context.sema);
			session_stop(&s_drbcc_thread_context);
			break;
		default:
			break;
	}
}

static void error_cb(void *context, char* msg)
{
	UNUSED(context);
	fprintf(stderr, "error_cb: %s\n", msg);
}


static void session_cb(void *context, DRBCC_SESSION_t session, int SUCCEEDED)
{
	UNUSED(context);
	if (SUCCEEDED)
	{
		TRACE(TL_DEBUG, "session_cb: %i succeeded", session);
	}
	else
	{
		TRACE(TL_DEBUG, "session_cb: %i failed", session);
	}
	session_stop(&s_drbcc_thread_context);
}

static void protocol_cb(void *context, int major, int minor, int fw_running, unsigned len, uint8_t* build_info)
{
	UNUSED(context);
	fprintf(stdout, "protocol_cb: BCTRL is running %s version %d.%02d",
		(fw_running?"FIRMWARE":"BOOTLOADER"), major, minor);
	if ((len > 0) && (build_info != NULL)) // there is additional build info data
	{
		char fw_buildinfo[17];
		int l;
		l = (len > 16) ? 16 : len;
		strncpy(fw_buildinfo, (char*)build_info, l);
		fprintf(stdout, ", fw_build_info: <%s>", fw_buildinfo);
		if (len >= 18) // there is also extra bootloader version info
		{
			fprintf(stdout, ", Bootloader version: %d.%02d", build_info[16], build_info[17]);
			if (len > 18) // there is also extra bootloader build info
			{
				fprintf(stdout, ", bl_build_info: <%s>", (build_info + 18));
			}
		}
	}
	fprintf(stdout, "\n");
}

static void id_cb(void *context, int board, int slot, unsigned len, uint8_t* ser)
{
	UNUSED(context);
	unsigned int i;
	fprintf(stdout, "id_cb: BoardID: %u, SlotID: %u, HW S/N: ", board, slot);
	for (i = 0; i < len; i++)
	{
		fprintf(stdout, "%02X ", ser[i]);
	}
	fprintf(stdout, "\n");
}

static void part_cb(void *context, unsigned num, DRBCC_PARTENTRY_t entries[])
{
	UNUSED(context);
	unsigned i;
	for(i = 0; i < num;i++)
	{
		fprintf(stdout, "part_cb: typeinfo=0x%02X start=0x%04X len=0x%06X\n",
				entries[i].type.typeinfo,
				entries[i].startblock,
				entries[i].length);
	}
}

static void rtc_cb(void *context, const struct tm* utctime, int epoch)
{
	UNUSED(context);
	char timestr[256];
	memset(timestr, 0, sizeof(timestr));
	if(strftime(timestr, sizeof(timestr)-1, "%Y-%m-%d %H:%M:%S", utctime) == 19 /* sizeof resulting time string */)
	{
		fprintf(stdout, "rtc_cb: %s (epoch=%u)\n", timestr, epoch);
	}
	else
	{
		fprintf(stdout, "rtc_cb: ERROR, could not parse struct tm data (%d %d %d %d %d %d %d %d %d epoch=%u)\n",
				utctime->tm_sec,
				utctime->tm_min,
				utctime->tm_hour,
				utctime->tm_mday,
				utctime->tm_mon,
				utctime->tm_year,
				utctime->tm_wday,
				utctime->tm_yday,
				utctime->tm_isdst,
				epoch);
	}
}

static void getlog_cb(void *context, int pos, int len, uint8_t data[])
{
	context_t* c = (context_t*)context;
	FILE *fp;
	int filecloseflag = 0;

	if (c->flashread_filename[0] != '\0')
	{
		// write output to file
		fp = fopen(c->flashread_filename, "a");
		if (fp != NULL)
		{
			filecloseflag = 1; // we need to close fd at end of callback
/*
			if (-1 == lseek(fp, 0, SEEK_END))
			{
				TRACE(TL_INFO, "lseek to END in file %s failed", c->flashread_filename);
				fclose(fp);
				fp = stdout; // use stdout instead for output
			}
			else
			{
				TRACE(TL_INFO, "lseek to END in file %s succeeded", c->flashread_filename);
				filecloseflag = 1; // we need to close fd at end of callback
			}
*/
		}
		else
		{
			TRACE(TL_INFO, "open file %s for writing flash content failed", c->flashread_filename);
			fp = stdout; // use stdout instead for output
		}
	}
	else
	{
		fp = stdout;
	}

	// hexdump to stdout or file
	int i;
//		char timestr[256];
//		memset(timestr, 0, sizeof(timestr));
//		strftime(timestr, sizeof(timestr)-1, "%Y-%m-%d %H:%M:%S", utctime);
	fprintf(fp, "log %6d: ", pos);

	if ((data[0] != DRBCC_E_EXTENSION) && (data[0] != DRBCC_E_EMPTY)) // no timestamp for extension data and empty entries
	{
		fprintf(fp, "20%02X-%02X-%02X %02X:%02X:%02X (epoch %u): ", data[7], data[6], data[5], data[3], data[2], data[1], data[4]);
	}

	switch(data[0])
	{
		case DRBCC_E_EXTENSION: // indicates that this log entry is a data extension of a previous log entry, it contains no timestamp
			fprintf(fp, "Extension data");
			break;

		case DRBCC_E_RAMLOG_OVERRUN: //  overrun occured in RAM logging
			fprintf(fp, "Ramlog overrun");
			break;

		case DRBCC_E_ILL_BID: // illegal board revision ID detected
			fprintf(fp, "Illegal board ID: 0x%02X", data[9]);
			break;

		case DRBCC_E_ILL_PWR_STATE: // illegal power state
			fprintf(fp, "Illegal power state: ");
			switch (data[9] & DRBCC_P_MAIN_STATE_MASK)
			{
				case DRBCC_P_UNKNOWN: // power state unknown (just initialized after reset)
					fprintf(fp, "UNKNOWN");
					break;

				case DRBCC_P_LITHIUM_POWER: // neither standby nor key power present (lithium powered)
					fprintf(fp, "LI_BATT");
					break;

				case DRBCC_P_KEY_POWER: // VKey activated, VKey-Bypass-FET closed (/VKEY_FET)
					fprintf(fp, "KEY_PWR");
					break;

				case DRBCC_P_STANDBY_POWER: // StandBy voltage present, standby-bypass-FET closed (/STBY_FET)
					fprintf(fp, "STANDBY");
					break;

				case DRBCC_P_HOST_POWERED: // host application powered (PWR_EN)
					fprintf(fp, "HOST_ON");
					break;

				default:
					fprintf(fp, "unknown (%d)", data[9] & DRBCC_P_MAIN_STATE_MASK);
					break;
			}

			if (data[9] & DRBCC_P_OPT_KEY_POWER_ENABLED) // VKey activated (VKEY_PWR_EN)
			{
				fprintf(fp, "|KEY_BAT");
			}

			if (data[9] & DRBCC_P_OPT_EXTENDED_PWR) // additional chips activated (/3V3S_EN)
			{
				fprintf(fp, "|EXT_PWR");
			}

			if (data[9] & DRBCC_P_OPT_DCDC_PWR) // Recom DC-DC converter activated (/DCDC_EN)
			{
				fprintf(fp, "|DCDC_ON");
			}

			if (data[9] & DRBCC_P_OPT_HDD_PWR) // HDD activated (HDD_PWR_EN)
			{
				fprintf(fp, "|HDD_PWR");
			}

			if (data[9] & DRBCC_P_OPT_LOCK_CHG) // enable the HDD lock charger (also necessary for P_KEY_POWER and P_OPT_KEY_POWER_ENABLED)
			{
				fprintf(fp, "|MAG_CHG");
			}

			break;

		case DRBCC_E_PWR_LOSS: // power loss
			fprintf(fp, "Power loss, reason: ");
			switch(data[9])
			{
				case DRBCC_PWR_LOSS_VKEY_TOOLOW:
				case DRBCC_PWR_LOSS_VKEY_EJCT_LOW:
				case DRBCC_PWR_LOSS_VKEY_LOCK:
				case DRBCC_PWR_LOSS_VKEY_RUN:
					fprintf(fp, "key battery too weak ");
					break;

				case DRBCC_PWR_FAILURE_HOST:
					fprintf(fp, "error in HOST voltages ");
					break;

				case DRBCC_PWR_LOSS_MAIN:
					fprintf(fp, "main power undervolt");
					break;

				case DRBCC_PWR_LOSS_PREALERT:
					fprintf(fp, "main power undervolt prealert");
					break;

				case DRBCC_PWR_LOSS_SUPERCAP:
					fprintf(fp, "supercap undervolt");
					break;

				default:
					fprintf(fp, "unknown (0x%02X) ", data[9]);
					break;
			}
			break;

		case DRBCC_E_HOST_LOGENTRY: // entry made by host
			fprintf(fp, "Host Logentry");
			break;

		case DRBCC_E_PWR_CHNG: // power state changed
			fprintf(fp, "Power state: ");
			switch (data[9] & DRBCC_P_MAIN_STATE_MASK)
			{
				case DRBCC_P_UNKNOWN: // power state unknown (just initialized after reset)
					fprintf(fp, "UNKNOWN");
					break;

				case DRBCC_P_LITHIUM_POWER: // neither standby nor key power present (lithium powered)
					fprintf(fp, "LI_BATT");
					break;

				case DRBCC_P_KEY_POWER: // VKey activated, VKey-Bypass-FET closed (/VKEY_FET)
					fprintf(fp, "KEY_PWR");
					break;

				case DRBCC_P_STANDBY_POWER: // StandBy voltage present, standby-bypass-FET closed (/STBY_FET)
					fprintf(fp, "STANDBY");
					break;

				case DRBCC_P_HOST_POWERED: // host application powered (PWR_EN)
					fprintf(fp, "HOST_ON");
					break;

				default:
					fprintf(fp, "unknown (%d)", data[9] & DRBCC_P_MAIN_STATE_MASK);
					break;
			}

			if (data[9] & DRBCC_P_OPT_KEY_POWER_ENABLED) // VKey activated (VKEY_PWR_EN)
			{
				fprintf(fp, "|KEY_BATT");
			}

			if (data[9] & DRBCC_P_OPT_EXTENDED_PWR) // additional chips activated (/3V3S_EN)
			{
				fprintf(fp, "|EXT_PWR");
			}

			if (data[9] & DRBCC_P_OPT_DCDC_PWR) // Recom DC-DC converter activated (/DCDC_EN)
			{
				fprintf(fp, "|DCDC_PWR");
			}

			if (data[9] & DRBCC_P_OPT_HDD_PWR) // HDD activated (HDD_PWR_EN)
			{
				fprintf(fp, "|HDD_PWR");
			}

			if (data[9] & DRBCC_P_OPT_LOCK_CHG) // enable the HDD lock charger (also necessary for P_KEY_POWER and P_OPT_KEY_POWER_ENABLED)
			{
				fprintf(fp, "|LOCK_CHG");
			}

			break;

		case DRBCC_E_ILL_INT: // unknown interrupt occured (woke us from power down)
			fprintf(fp, "Unknown interrupt");
			break;

		case DRBCC_E_HDD_CHNG: // state of HDD sensor changed
			fprintf(fp, "HDD %s", (data[9] != 0) ? "inserted" : "removed ");
			break;

		case DRBCC_E_KEY_DETECTED: // key was detected, param: key eeprom code (7 byte)
			fprintf(fp, "HD-Key detected, ID: ");
			for (i = 9; i < 16; i++)
			{
				fprintf(fp, "%02X ", data[i]);
			}
			break;

		case DRBCC_E_KEY_REJECTED: // key was rejected, no applicable token was found, param: number of tokens (1 byte)
			fprintf(fp, "HD-Key rejected, %d token parsed", data[9]);
			break;

		case DRBCC_E_KEY_SUCCESS: // key was successfully processed, param: begin of token (6 byte) + number of eject retries (1 byte)
			if (DRBCC_KEY_CMD0_TTU_EJECT == data[9])
			{
				fprintf(fp, "TTU HD eject success, retries: %d", data[15]);
			}
			else
			{
				fprintf(fp, "HD-Key success, Token bits:");
				if (data[9] & DRBCC_KEY_CMD0_MASK_EJECT)   fprintf(fp, " EJECT");
				if (data[9] & DRBCC_KEY_CMD0_MASK_CLEAR)   fprintf(fp, " CLEAR");
				if (data[9] & DRBCC_KEY_CMD0_MASK_ADAPT)   fprintf(fp, " ADAPT");
				if (data[9] & DRBCC_KEY_CMD0_MASK_NOCOMP)  fprintf(fp, " NOCOMP");
				if (data[9] & DRBCC_KEY_CMD0_MASK_EXPDATE) fprintf(fp, " EXPIRES");
				fprintf(fp, " Code: %02X%02X%02X-%02X, retries: %d", data[11], data[12], data[13], data[14], data[15]);
			}
			break;

		case DRBCC_E_UNLOCK_ERROR: // hdd unlock failed, param: begin of token (6 byte) + number of eject retries (1 byte)
			if (DRBCC_KEY_CMD0_TTU_EJECT == data[9])
			{
				fprintf(fp, "TTU HD eject failed, retries: %d", data[15]);
			}
			else
			{
				fprintf(fp, "HD-Key unlock failed, Token bits:");
				if (data[9] & DRBCC_KEY_CMD0_MASK_EJECT)   fprintf(fp, " EJECT");
				if (data[9] & DRBCC_KEY_CMD0_MASK_CLEAR)   fprintf(fp, " CLEAR");
				if (data[9] & DRBCC_KEY_CMD0_MASK_ADAPT)   fprintf(fp, " ADAPT");
				if (data[9] & DRBCC_KEY_CMD0_MASK_NOCOMP)  fprintf(fp, " NOCOMP");
				if (data[9] & DRBCC_KEY_CMD0_MASK_EXPDATE) fprintf(fp, " EXPIRES");
				fprintf(fp, " Code: %02X%02X%02X-%02X, retries: %d", data[11], data[12], data[13], data[14], data[15]);
			}
			break;

		case DRBCC_E_KEY_COMM_ERROR: // key handling failed: invalid key eeprom code,  param: key eeprom code (7 byte)
			fprintf(fp, "HD-Key communication error, ID: ");
			for (i = 9; i < 16; i++)
			{
				fprintf(fp, "%02X ", data[i]);
			}
			break;

		case DRBCC_E_KEY_HEADER_ERROR: // key handling failed: error in key header data, param: begin of key header data (4 byte)
			fprintf(fp, "HD-Key header error, header data: 0x%02X 0x%02X 0x%02X 0x%02X ", data[9], data[10], data[11], data[12]);
			break;

		case DRBCC_E_RTC_SET: // RTC willbe set to new time (caused epoch change); timestamp is old time, param: new time
			fprintf(fp, "RTC set to 20%02X-%02X-%02X %02X:%02X:%02X", data[15], data[14], data[13], data[11], data[10], data[9]);
			break;

		case DRBCC_E_COMM_TIMEOUT: // HOST Communication Heartbeat timeout
			switch (data[9])
			{
				case DRBCC_TIMEOUT_FIRST_MESSAGE:
					fprintf(fp, "HOST First message timeout");
					break;

				case DRBCC_TIMEOUT_HEARTBEAT:
					fprintf(fp, "HOST Heartbeat timeout");
					break;

				case DRBCC_TIMEOUT_SHUTDOWN:
					fprintf(fp, "HOST Shutdown timeout");
					break;

				default:
					fprintf(fp, "HOST Communication timeout 0x%02X", data[9]);
					break;
			}
			break;

		case DRBCC_E_VOLTAGE_INFO: // List of internal voltage values; param: one or more pairs of voltage ID (1 byte enum) and value in 10mV (2 byte int signed)
			{
				unsigned int len = data[8];
				unsigned int pos = 9;
				uint16_t h;
				int16_t x;
				float v;

				fprintf(fp, "Voltage data: ");
				while (len >= 3)
				{
					switch (data[pos])
					{
						case DRBCC_VID_POWER_FILTER: // power filter input voltage
							fprintf(fp, "PFLT=");
							break;

						case DRBCC_VID_POWER_CAP: // power capacitor input voltage
							fprintf(fp, "PCAP=");
							break;

						case DRBCC_VID_PWR_CAM: // camera power terminal voltage
							fprintf(fp, "PCAM=");
							break;

						case DRBCC_VID_VKEY: // vkey step up supply voltage
							fprintf(fp, "VKEY=");
							break;

						case DRBCC_VID_SUPERCAP: // HDD supply supercap voltage
							fprintf(fp, "SCAP=");
							break;

						case DRBCC_VID_12V: // 12V camera power supply voltage
							fprintf(fp, "12V=");
							break;

						case DRBCC_VID_5V: // 5V supply voltage
							fprintf(fp, "5V=");
							break;

						case DRBCC_VID_VDD: // switched 3.3V supply voltage
							fprintf(fp, "VDD=");
							break;

						case DRBCC_VID_1V8: // 1.8V supply voltage
							fprintf(fp, "1V8=");
							break;

						case DRBCC_VID_1V2: // 1.2V supply voltage
							fprintf(fp, "1V2=");
							break;

						case DRBCC_VID_1V: // 1.0V supply voltage
							fprintf(fp, "1V0=");
							break;

						case DRBCC_VID_3V3D: // 3.3V BCTRL supply voltage
							fprintf(fp, "3V3D=");
							break;

						case DRBCC_VID_1V5: // 1.5V supply voltage (new with hidav)
							fprintf(fp, "1V5=");
							break;

						case DRBCC_VID_TERM: // DRAM term power voltage (new with hidav)
							fprintf(fp, "TERM=");
							break;

						case DRBCC_VID_VBAT: // Li-Batt Voltage (remembered value from when last running on battery)
							fprintf(fp, "VBAT=");
							break;

						default: // unknown voltage ID
							fprintf(fp, "VID%d=", data[pos]);
							break;
					}
					h = 256 * data[pos+1] + data[pos+2];
					x = (int16_t)h;
					v = x;
					fprintf(fp, "%.2fV ", v/100);
					pos += 3;
					len -= 3;
				}
			}
			break;

		case DRBCC_E_LOG_CLEARED: // Ringlog was cleared
			fprintf(fp, "Log memory cleared");
			break;

		case DRBCC_E_HDD_USABLE_ON: // HDD_USABLE signal was turned on (HD may now be used)
			fprintf(fp, "HDD_USABLE on (ready for HDD use now)");
			break;

		case DRBCC_E_FW_UPDATE: // firmware update performed , Param: result: 0=error 1=ok
			fprintf(fp, "BCTRL firmware update %s", (data[9] != 0) ? "successful":"failed");
			break;

		case DRBCC_E_BL_UPDATE: // bootloader update performed, Param: result: 0=error 1=ok
			fprintf(fp, "BCTRL bootloader update %s", (data[9] != 0) ? "successful":"failed");
			break;

		case DRBCC_E_FW_REBOOT: // firmware reboot (e.g. in order to do a fw update) initiated, Param: 1= with 0=without SRAM context reset
			fprintf(fp, "Forced firmware reboot %s SRAM context reset", (data[9] != 0) ? "with":"without");
			break;

		case DRBCC_E_OVERTEMP_OFF: // emergency turn off: hard high temperature limit exceeded, Param: temp (int8)
			fprintf(fp, "Overtemperature turn off at %d deg C", (int8_t)data[9]);
			break;

		case DRBCC_E_TEMPLIMIT: // could not turn on because of temp outside safe limits, Param: temp , lower limit, upper limit, reset limit (alle int8)
			fprintf(fp, "Could not turn on: temp %d deg C not between %d and %d, reset below %d",
			        (int8_t)data[9], (int8_t)data[10], (int8_t)data[11], (int8_t)data[12]);
			break;

		case DRBCC_E_ACCEL_EVENT:      // acceleration sensor event, Params: Event type (byte), accel data (6 byte: xx, yy, zz)
			{
				uint16_t h;
				int16_t x,y,z;
				float fx, fy, fz;
				float accel;

				h = data[11] * 256 + data[10];
				x = (int16_t)h; fx = x; fx *= 1000; fx /= 256; x = (int16_t)fx;
				h = data[13] * 256 + data[12];
				y = (int16_t)h; fy = y; fy *= 1000; fy /= 256; y = (int16_t)fy;
				h = data[15] * 256 + data[14];
				z = (int16_t)h; fz = z; fz *= 1000; fz /= 256; z = (int16_t)fz;

				accel = sqrt(fx*fx + fy*fy + fz*fz)/1000;

				fprintf(fp, "Accel event type %d: %s, %.2fg, (xyz: %d,%d,%d mg)",
				        data[9], (data[9] == DRBCC_ACCEL_EVENT_THRESHOLD_HIGH) ? "THRESHOLD_HIGH" : "UNKNOWN",
				        accel, x, y, z);
			}
			break;

		case 0xff:
			fprintf(fp, "EMPTY LOGENTRY");
			break;

		default:
			fprintf(fp, "UNKNOWN LOGENTRY 0x%02X", data[0]);
			break;
	}

	// raw data dump
	if (c->raw != 0)
	{
		fprintf(fp, " ---------- RAW len %d data: ", len);
		for (i = 0; i < len; i++)
		{
			fprintf(fp, "%02X ", data[i]);
		}
	}
	fprintf(fp, "\n");

	if (filecloseflag != 0)
	{
		fflush(fp);
		fclose(fp);
	}
}

static void getpos_cb(void *context, int pos, uint8_t entry, uint8_t wrapflag)
{
	UNUSED(context);
	fprintf(stdout, "pos: 0x%02X entry: 0x%02X wrapflag: 0x%02X\n", pos, entry, wrapflag);
	fflush(stdout);
}

static void debug_get_cb(void *context, unsigned int addr, int len, uint8_t data[])
{
	UNUSED(context);
	fprintf(stdout, "debug_get_cb: addr: 0x%08X len: %d data: ", addr, len);
	if(len > 0)
	{
		int i;
		for(i=0; i<len; ++i)
		{
			fprintf(stdout, "%02X ", data[i]);
		}
	}
	fprintf(stdout, "\n");
}

static void status_cb(void *context, unsigned len, uint8_t* state)
{
	UNUSED(context);
	unsigned int i;
	int16_t x, y, z;
	float v, fh;
	uint16_t h;
	fprintf(stdout, "status_cb: raw data: ");
	for (i = 0; i < len; i++)
	{
		fprintf(stdout, "%02x ", state[i]);
	}

	fprintf(stdout, "\n");
	fprintf(stdout, "SPIs/SPOs: Ignition: %s, ", (state[0] & DRBCC_SFI_IGNITION_bm) ? "on":"off");
	fprintf(stdout, "HDD-Sense: %s, ", (state[0] & DRBCC_SFI_HDD_SENS_bm) ? "open":"closed");
	fprintf(stdout, "OXE_INT1: %s, ", (state[0] & DRBCC_SFI_OXE_INT1_bm) ? "1":"0");
	fprintf(stdout, "OXE_INT2: %s, ", (state[0] & DRBCC_SFI_OXE_INT2_bm) ? "1":"0");
	fprintf(stdout, "XOSC_ERR: %s, ", (state[0] & DRBCC_SFI_XOSC_ERR_bm) ? "1":"0");
	fprintf(stdout, "GPI-Power: %s, ", (state[2] & DRBCC_SFO_GPIPWR_bm) ? "on":"off");
	fprintf(stdout, "HDD-Power: %s\n", (state[2] & DRBCC_SFO_HDDPWR_bm) ? "on":"off");
	fprintf(stdout, "GPInputs 1..6: ");
	for (i=0; i<6; i++) fprintf(stdout, "%i ", (state[3] & (1<<i)) ? 1 : 0);
	fprintf(stdout, "  GPOutputs 1..4: ");
	for (i=0; i<4; i++) fprintf(stdout, "%i ", (state[5] & (1<<i)) ? 1 : 0);
	fprintf(stdout, " RTC Temp : %i deg C ", (int8_t)state[6]);
	if (len > 7)
	{
		h = 256 * state[9] + state[8];
		x = (int16_t)h; fh = x; fh *= 1000; fh /= 256; x = (int16_t)fh;
		h = 256 * state[11] + state[10];
		y = (int16_t)h; fh = y; fh *= 1000; fh /= 256; y = (int16_t)fh;
		h = 256 * state[13] + state[12];
		z = (int16_t)h; fh = z; fh *= 1000; fh /= 256; z = (int16_t)fh;
		fprintf(stdout, " Accel X, Y, Z: %img, %img, %img", x,y,z);
	}
	fprintf(stdout, "\n");

	if (len > 29)
	{
		fprintf(stdout, "Voltages: ");
		for (i = 0; i < 11; i++)
		{
			h = 256 * state[14 + 2*i] + state[15 + 2*i];
			x = (int16_t)h;
			v = x;
			fprintf(stdout, "%.2fV  ", v/100);
		}
	}
	fprintf(stdout, "\n");
}

static void accel_event_cb(void *context, DRBCC_Accel_Events_t type, int16_t x, int16_t y, int16_t z)
{
	UNUSED(context);
	float fh;
	fh = x; fh *= 1000; fh /= 256; x = (int16_t)fh;
	fh = y; fh *= 1000; fh /= 256; y = (int16_t)fh;
	fh = z; fh *= 1000; fh /= 256; z = (int16_t)fh;
	fprintf(stdout, "accel_event_cb: Accel type, X, Y, Z: %d(%s) %img %img %img\n", type, (DRBCC_ACCEL_EVENT_THRESHOLD_HIGH == type) ? "THRESHOLD_HIGH" : "unknown", x, y, z);
}

static void flash_id_cb(void *context, uint8_t mid, uint8_t devid1, uint8_t devid2)
{
	UNUSED(context);
	fprintf(stdout, "flash_id_cb: ManufacturerID=%02X DeviceID=%02X%02X\n", mid, devid1, devid2);
}

static void flash_read_cb(void *context, unsigned addr, unsigned len, uint8_t* data)
{
	context_t* c = (context_t*)context;
	if (c->flashread_filename[0] == '\0')
	{
		// hexdump to stdout
		unsigned i = 0;
		for(i = 0; i < len; i++)
		{
			if((i % 16) == 0)
			{
				fprintf(stdout, "0x%04X:", i + addr);
			}
			fprintf(stdout, " %02X", data[i]);
			if((i % 16) == 15)
			{
				fprintf(stdout, "\n");
			}
		}
		if(i % 16)
		{
			fprintf(stdout, "\n");
		}
	}
	else
	{
		// write binary to file
		int fd = open(c->flashread_filename, O_RDWR | O_CREAT | OX_BINARY, 0666 );
		if(fd != -1)
		{
			ssize_t wr_ret = len;
			if(-1 == lseek(fd, addr - c->readoffset, SEEK_SET))
			{
				TRACE(TL_INFO, "lseek to offset %lu in file %s failed", addr - c->readoffset, c->flashread_filename);
			}
			else if(wr_ret == write(fd, data, len))
			{
				struct timeval now;
				struct timeval delta;
				gettimeofday(&now, NULL);

				timersub(&now, &(c->tm_flashread_request), &delta);
				fprintf(stdout, "flash_read_cb: after %ld.%06ld seconds addr=%u len=%u data written to file %s\n", delta.tv_sec, delta.tv_usec, addr, len, c->flashread_filename);
			}
			else
			{
				TRACE(TL_INFO, "writing flash data to file %s failed", c->flashread_filename);
			}
			close(fd);
		}
		else
		{
			TRACE(TL_INFO, "open file %s for writing flash content failed", c->flashread_filename);
		}
	}
}

static void flash_write_cb(void *context, unsigned addr, unsigned len, uint8_t result)
{
	UNUSED(context);
	fprintf(stdout, "flash_write_cb: addr=%u len=%u result=0x%02X\n", addr, len, result);
}

static void flash_erase_cb(void *context, unsigned blocknum, uint8_t result)
{
	UNUSED(context);
	fprintf(stdout, "flash_erase_cb: block=%u result=0x%02X\n", blocknum, result);
}
static void progress_cb(void *context, int cur, int max)
{
	UNUSED(context);
	fprintf(stdout, "progress_cb: current=%i of %i\n", cur, max);
}

static int get_command(DRBCC_thread_context_t *drbcc_thread, const char **line, cmdid_t* cmd)
{
	unsigned i = 0;

	if(0 == drbcc_thread->current_line)
	{
		// stop signal
		drbcc_thread->running = 0;
		TRACE(TL_DEBUG, "get_command() stop thread.");
		return -1;
	}
	if(0 == *drbcc_thread->current_line)
	{
		return 0;
	}
	if(0 == cmd)
	{
		TRACE(TL_INFO, "get_command() parameter cmd is 0.");
		return 0;
	}
	*cmd = cmdid_unknown;
	if(drbcc_thread->line_valid)
	{
		drbcc_thread->line_valid = 0;
		*line = drbcc_thread->current_line;
		TRACE(TL_DEBUG, "get_command() got cmd: %s.", *line);
		while(s_cmds[i].id != cmdid_unknown)
		{
			if(0 == strncasecmp(*line, s_cmds[i].cmdtext, s_cmds[i].matchlen))
			{
				*cmd = s_cmds[i].id;
				break;
			}
			i++;
		}
		return 1;
	}
	return 0;
}

static void put_command(DRBCC_thread_context_t *drbcc_thread, const char * line)
{
	TRACE(TL_DEBUG, "put cmd: %s.", line);
	drbcc_thread->current_line = line;
	drbcc_thread->line_valid = 1;
}

static void execute_cmd_line(const char *line)
{
	put_command(&s_drbcc_thread_context, line);
	if(drbcc_sema_wait(s_drbcc_thread_context.sema) < 0)
	{
		TRACE(TL_INFO, "wait drbcc cmd failed. exit process.");
		s_running = 0;
	}
}

static void register_flash_cbs(DRBCC_HANDLE_t h)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_read_cb, (h, flash_read_cb));
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_write_cb, (h, flash_write_cb));
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_erase_cb, (h, flash_erase_cb));
}

static void unregister_flash_cbs(DRBCC_HANDLE_t h)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_read_cb, (h, NULL));
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_write_cb, (h, NULL));
	CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_erase_cb, (h, NULL));
}

static void process_cmd_setgpo(DRBCC_HANDLE_t h, DRBCC_thread_context_t * drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int gpo = (unsigned int)-1;
	int onoff = 0xff;
	if(2 != sscanf(line, "%*s%u,%d", &gpo, &onoff))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_set_gpo, (h, &session,  gpo, onoff));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_setled(DRBCC_HANDLE_t h, DRBCC_thread_context_t * drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int num = (unsigned int)-1;
	int color = 0xff;
	unsigned int on_time  = 0;
	unsigned int off_time = 0;
	unsigned int phase    = 0;
	int result;

	result = sscanf(line, "%*s%u,%d,%d,%d,%d", &num, &color, &on_time, &off_time, &phase);
	if(result < 2)
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_set_led, (h, &session, num, color, on_time, off_time, phase));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_getstatus(DRBCC_HANDLE_t h, DRBCC_thread_context_t * drbcc_thread, const char* line)
{
	UNUSED(line);
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	session_start(drbcc_thread);
	CHECKCALL(TL_DEBUG, rc, drbcc_get_status, (h, &session));
	if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
}

static void process_cmd_getiddata(DRBCC_HANDLE_t h, DRBCC_thread_context_t * drbcc_thread, const char* line)
{
	UNUSED(line);
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	session_start(drbcc_thread);
	CHECKCALL(TL_DEBUG, rc, drbcc_get_id_data, (h, &session));
	if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
}

static void process_cmd_setrtc(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread,  const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	char* timestr = NULL;
	struct tm t;

	timestr = strchr(line, ' ');
	if(timestr)
	{
		time_t bigbang = 0; // defaults
		gmtime_r(&bigbang, &t);
		strptime(timestr + 1, "%Y-%m-%d %H:%M:%S", &t);
	}
	else
	{
		time_t now = time(NULL);
		gmtime_r(&now, &t);
	}
	session_start(drbcc_thread);
	CHECKCALL(TL_DEBUG, rc, drbcc_set_rtc, (h, &session,  &t));
	if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
}

static void process_cmd_rflash(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int addr = (unsigned int)-1;
	unsigned int len = 0;
	s_context.flashread_filename[0] = '\0';
	gettimeofday(&(s_context.tm_flashread_request), NULL);

	if(2 >  sscanf(line, "%*s%i,%i,%s", &addr, &len, s_context.flashread_filename))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		register_flash_cbs(h);
		s_context.readoffset = addr;
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_req_flash_read, (h, &session, addr, len));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_wflash(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int addr = (unsigned int)-1;
	ssize_t len = 0;
	char file[FILENAME_MAX] = "";
	sscanf(line, "%*s%i,%zi,%s", &addr, &len, file);
	if(3 != sscanf(line, "%*s%i,%zi,%s", &addr, &len, file))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		uint8_t* buf = (uint8_t*)malloc(len);
		if(buf)
		{
			int fd = open(file, O_RDONLY | OX_BINARY);
			memset(buf, 0xFF, len);
			if(fd != -1)
			{
				if(len == read(fd, buf, len))
				{
					register_flash_cbs(h);
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_req_flash_write, (h, &session, addr, len, buf));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				else
				{
					TRACE(TL_INFO, "reading flash data from file %s failed", file);
					drbcc_sema_release(drbcc_thread->sema);
				}
				close(fd);
			}
			else
			{
				TRACE(TL_INFO, "open file %s for reading flash content failed", file);
				drbcc_sema_release(drbcc_thread->sema);
			}
			free(buf);
		}
	}
}

static void process_cmd_eflash(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int block = (unsigned int)-1;
	if(1 != sscanf(line, "%*s%i", &block))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		register_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_req_flash_erase_block, (h, &session, block));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_gfile(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int index = (unsigned int)-1;
	char path[FILENAME_MAX] = "";
	if(2 != sscanf(line, "%*s%u,%s", &index, path))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_get_file, (h, &session, index, path));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_gfiletype(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int index = (unsigned int)-1;
	char path[FILENAME_MAX] = "";
	if(2 != sscanf(line, "%*s%i,%s", &index, path))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_get_file_type, (h, &session, index, path));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_glog(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	int entries = 1000000; // default all

	s_context.raw = 0; // default: no raw output
	s_context.flashread_filename[0] = '\0'; // default: output to stdout

	if(0 == sscanf(line, "%*s %d,%d,%s", &entries, &s_context.raw, s_context.flashread_filename))
	{
		sscanf(line, "%*s ,%d,%s", &s_context.raw, s_context.flashread_filename);
	}
	unregister_flash_cbs(h);
	session_start(drbcc_thread);
	//CHECKCALL(TL_DEBUG, rc, drbcc_get_log, (h, &session, 1, &start, &end));	// ring log with start stop time
	//CHECKCALL(TL_DEBUG, rc, drbcc_get_log, (h, &session, 0, NULL, NULL));	// persistent log
	CHECKCALL(TL_DEBUG, rc, drbcc_get_log, (h, &session, 1, entries));	// ring log
	if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
}

static void process_cmd_pfile(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int index = (unsigned int)-1;
	unsigned int type = (unsigned int)-1;
	char path[FILENAME_MAX] = "";
	if(3 != sscanf(line, "%*s%u,%u,%s", &index, &type, path))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_put_file, (h, &session, index, type, path));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_pfiletype(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int type = (unsigned int)-1;
	char path[FILENAME_MAX] = "";
	if(2 != sscanf(line, "%*s%i,%s", &type, path))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_put_file_type, (h, &session, type, path));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_dfile(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int index = (unsigned int)-1;
	if(1 != sscanf(line, "%*s%u", &index))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_delete_file, (h, &session, index));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_dfiletype(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	unsigned int index = (unsigned int)-1;
	if(1 != sscanf(line, "%*s%i", &index))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_delete_file_type, (h, &session, index));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_fwupl(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	char fwpath[FILENAME_MAX] = "";
	if(1 != sscanf(line, "%*s%s", fwpath))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_upload_firmware, (h, &session, fwpath));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_cmd_blupl(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread, const char* line)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	char fwpath[FILENAME_MAX] = "";
	if(1 != sscanf(line, "%*s%s", fwpath))
	{
		TRACE(TL_INFO, "command syntax problem: %s", line);
		drbcc_sema_release(drbcc_thread->sema);
	}
	else
	{
		unregister_flash_cbs(h);
		session_start(drbcc_thread);
		CHECKCALL(TL_DEBUG, rc, drbcc_upload_bootloader, (h, &session, fwpath));
		if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
	}
}

static void process_input(DRBCC_HANDLE_t h, DRBCC_thread_context_t *drbcc_thread)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;

	static struct timeval wait_time = { 0, 0 };
	struct timeval now;
	struct timeval dur;
	const char* line = 0;
	cmdid_t cmd;

	gettimeofday(&now, NULL);

	if(timercmp(&now, &wait_time, >) && 1 <= get_command(drbcc_thread, &line, &cmd))
	{
		TRACE(TL_DEBUG, "handle cmd: %d, line: %s", cmd, line);
		switch(cmd)
		{
			case cmdid_hdeject:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_eject_hd, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_hdonoff:
			{
				unsigned d = 0; // on/off
				if(1 != sscanf(line, "%*s%u", &d))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_hd_power, (h, &session, d));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_gpipower:
			{
				unsigned d = 0; // on/off
				if(1 != sscanf(line, "%*s%u", &d))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_gpi_power, (h, &session, d));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_wait:
			{
				unsigned long d = 0; // milliseconds
				if(1 != sscanf(line, "%*s%lu", &d))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{

					dur.tv_sec  = d / 1000;
					dur.tv_usec = d % 1000;

					timeradd(&now, &dur, &wait_time);
					TRACE(TL_DEBUG, "waiting for %ld.%06ld secs till %ld.%06ld", dur.tv_sec, dur.tv_usec, wait_time.tv_sec, wait_time.tv_usec);
					drbcc_sema_release(drbcc_thread->sema);
				}
				break;
			}
			case cmdid_shutdown:
			{
				unsigned d = 0; // seconds
				if(1 != sscanf(line, "%*s%u", &d))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_shutdown, (h, &session, d));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_heartbeat:
			{
				unsigned d = 0; // seconds
				if(1 != sscanf(line, "%*s%u", &d))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_heartbeat, (h, &session, d));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_exit:
				drbcc_thread->running = 0;
				//drbcc_sema_destroy(&drbcc_thread->sema);
				break;
			case cmdid_help:
				print_usage(stdout, 0);
				drbcc_sema_release(drbcc_thread->sema);
				break;
			case cmdid_sync:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_sync, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_proto:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_req_protocol, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_part:
				unregister_flash_cbs(h);
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_get_partitiontable, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_getrtc:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_req_rtc, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_setrtc:
				process_cmd_setrtc(h, drbcc_thread, line);
				break;
			case cmdid_setgpo:
				process_cmd_setgpo(h, drbcc_thread, line);
				break;
			case cmdid_setled:
				process_cmd_setled(h, drbcc_thread, line);
				break;
			case cmdid_getstatus:
				process_cmd_getstatus(h, drbcc_thread, line);
				break;
			case cmdid_getiddata:
				process_cmd_getiddata(h, drbcc_thread, line);
				break;
			case cmdid_flashid:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_req_flash_id, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_rflash:
				process_cmd_rflash(h, drbcc_thread, line);
				break;
			case cmdid_wflash:
				process_cmd_wflash(h, drbcc_thread, line);
				break;
			case cmdid_eflash:
				process_cmd_eflash(h, drbcc_thread, line);
				break;
			case cmdid_gfile:
				process_cmd_gfile(h, drbcc_thread, line);
				break;
			case cmdid_gfiletype:
				process_cmd_gfiletype(h, drbcc_thread, line);
				break;
			case cmdid_pfile:
				process_cmd_pfile(h, drbcc_thread, line);
				break;
			case cmdid_pfiletype:
				process_cmd_pfiletype(h, drbcc_thread, line);
				break;
			case cmdid_dfile:
				process_cmd_dfile(h, drbcc_thread, line);
				break;
			case cmdid_dfiletype:
				process_cmd_dfiletype(h, drbcc_thread, line);
				break;
			case cmdid_blupl:
				process_cmd_blupl(h, drbcc_thread, line);
				break;
			case cmdid_fwupl:
				process_cmd_fwupl(h, drbcc_thread, line);
				break;
			case cmdid_blupd:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_request_bootloader_update, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_fwinv:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_invalidate_bctrl_fw, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_restart:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_restart_bctrl, (h, &session, 0));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_getlog:
				process_cmd_glog(h, drbcc_thread, line);
				break;
			case cmdid_putlog:
			{
				unsigned char data[256];
				unsigned int i;
				unsigned d = 5;
				for (i=0; i<sizeof(data); ++i)
				{
					data[i] = i;
				}
				sscanf(line, "%*s%i", &d);
				if(d > 128) { d = 128; }
				if(d < 1)   { d = 1; }
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_put_log, (h, &session, 1, d, data));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
			}
				break;
			case cmdid_getpos:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_get_pos, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_clearlog:
				session_start(drbcc_thread);
				CHECKCALL(TL_DEBUG, rc, drbcc_clear_log, (h, &session));
				if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				break;
			case cmdid_debugset:
			{
				unsigned addr = 0;
				char dataHex[1024];
				memset(dataHex, 0, sizeof(dataHex));
				if(1 > sscanf(line, "%*s%u,%s", &addr, dataHex))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					uint8_t data[256];
					unsigned int i;
					size_t len = strlen(dataHex);
					for(i=0; (i < (len/2)) && (i<sizeof(data)); ++i)
					{
						int v = 0;
						char s[3];
						s[0] = dataHex[2*i];
						s[1] = dataHex[2*i+1];
						s[2] = 0;
						sscanf(s, "%2x", &v);
						data[i] = (uint8_t)v;
					}
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_debug_set, (h, &session, addr, i, data));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_debugget:
			{
				unsigned addr = 0;
				if(1 != sscanf(line, "%*s%u", &addr))
				{
					TRACE(TL_INFO, "command syntax problem: %s", line);
					drbcc_sema_release(drbcc_thread->sema);
				}
				else
				{
					session_start(drbcc_thread);
					CHECKCALL(TL_DEBUG, rc, drbcc_debug_get, (h, &session, addr));
					if(rc != DRBCC_RC_NOERROR) { session_stop(drbcc_thread); }
				}
				break;
			}
			case cmdid_unknown:
				TRACE(TL_INFO, "unknown command: %s", line);
			default:
				drbcc_sema_release(drbcc_thread->sema);
				TRACE(TL_TRACE, "ignoring unknown command: %s", line);
				break;
		}
	}
}

static void * drbcc_thread_main(void *arg)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	DRBCC_thread_context_t *drbcc_thread = (DRBCC_thread_context_t*)arg;
	DRBCC_HANDLE_t h = drbcc_thread->h;
	while(drbcc_thread->running || session_active(drbcc_thread))
	{
		struct timespec pause;

		CHECKCALL(TL_TRACE, rc, drbcc_trigger, (h, 1000));

		if(0 == session_active(drbcc_thread))
		{
			process_input(h, drbcc_thread);
		}

		pause.tv_sec = 0;
		pause.tv_nsec = 100000; // 100us
		nanosleep(&pause, NULL);
	}
	s_running = 0;
	drbcc_sema_release(drbcc_thread->sema);
	pthread_exit(0);
	return 0;
}

static DRBCC_RC_t run_connection(DRBCC_HANDLE_t h)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
        pthread_t drbcc_thread;
	char empty_cmd_line[] = { "" };

	memset(&s_drbcc_thread_context, 0, sizeof(s_drbcc_thread_context));
	s_drbcc_thread_context.current_line = empty_cmd_line;

	CHECKCALL(TL_DEBUG, rc, drbcc_start, (h, &s_context, s_dev, s_speed));
	if(rc == DRBCC_RC_NOERROR)
	{
		int ret;
		struct sigaction term_sa;
		struct sigaction term_oldsa;
		struct sigaction int_sa;
		struct sigaction int_oldsa;

		// install signal handlers for TERM and INT
		memset(&term_sa, 0, sizeof(term_sa));
		term_sa.sa_handler = sighandler;
		sigaction(SIGTERM, &term_sa, &term_oldsa);
		memset(&int_sa, 0, sizeof(int_sa));
		int_sa.sa_handler = sighandler;
		sigaction(SIGINT, &int_sa, &int_oldsa);

		if(1 != drbcc_sema_create(&s_drbcc_thread_context.sema))
		{
			TRACE(TL_INFO, "drbcc_sema_create failed.");
			return DRBCC_RC_NOERROR;
		}
		s_running = 1;
		s_drbcc_thread_context.running = 1;
		s_drbcc_thread_context.h = h;
		ret = pthread_create(&drbcc_thread, 0, drbcc_thread_main, &s_drbcc_thread_context);
		if (0 != ret)
		{
			TRACE(TL_INFO, "pthread_create failed with %d.", ret);
			return DRBCC_RC_NOERROR;
		}
		if(strlen(s_cmd_line) >= 1)
		{
			execute_cmd_line(s_cmd_line);
		}
		else
		{
			while(s_running)
			{
				char* line = 0;
				line = readline("drbcc> ");
				if(line && *line)
				{
					add_history(line);
					execute_cmd_line(line);
				}
				if(line)
				{
					free(line);
				}
				else
				{
					s_running = 0;
				}
			}
		}
		//exit thread
		s_drbcc_thread_context.current_line = 0;
		ret = pthread_join(drbcc_thread, 0);
		if (0 != ret)
		{
			TRACE(TL_INFO, "pthread_join failed with %d.", ret);
		}
		drbcc_sema_destroy(&s_drbcc_thread_context.sema);

		// restore signal handlers for TERM and INT
		sigaction(SIGTERM, &term_oldsa, NULL);
		sigaction(SIGINT, &int_oldsa, NULL);
	}
	CHECKCALL(TL_DEBUG, rc, drbcc_stop, (h));

	return rc;
}

static DRBCC_RC_t establish_connection()
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	DRBCC_HANDLE_t h = NULL;

	CHECKCALL(TL_DEBUG, rc, drbcc_open, (&h));
	if(rc == DRBCC_RC_NOERROR)
	{
		CHECKCALL(TL_DEBUG, rc, drbcc_register_error_cb, (h, error_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_session_cb, (h, session_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_protocol_cb, (h, protocol_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_id_cb, (h, id_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_partition_cb, (h, part_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_rtc_cb, (h, rtc_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_status_cb, (h, status_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_accel_event_cb, (h, accel_event_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_flash_id_cb, (h, flash_id_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_progress_cb, (h, progress_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_getlog_cb, (h, getlog_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_getpos_cb, (h, getpos_cb));
		CHECKCALL(TL_DEBUG, rc, drbcc_register_debug_get_cb, (h, debug_get_cb));
		register_flash_cbs(h);
		if(rc == DRBCC_RC_NOERROR)
		{
			CHECKCALL(TL_DEBUG, rc, run_connection, (h));
		}
	}
	CHECKCALL(TL_DEBUG, rc, drbcc_close, (h));

	return rc;
}

enum opts {
	opt_version,
	opt_cmd,
	opt_dev,
	opt_trace,
	opt_libtrace,
	opt_help
};

int main(int argc, char *argv[])
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	unsigned long libtracemask = 0;
#ifdef HAVE_LIBDRTRACE
	DRHIP_TRACE_INIT(1, 0xffffffff);
	DRHIP_TRACE_REGISTER_MODULE(drbcctrace, "drbcc");
	DRHIP_TRACE_SET_LVL(drbcctrace, DRHIP_TCAT_3, DRHIP_TLVL_1);
#endif
	int short_option;
	static int long_option;
	struct option longopts[] = {
		{"help",     no_argument,       &long_option, opt_help},
		{"version",  no_argument,       &long_option, opt_version},
		{"cmd",      required_argument, &long_option, opt_cmd},
		{"dev",      required_argument, &long_option, opt_dev},
		{"trace",    required_argument, &long_option, opt_trace},
		{"libtrace", required_argument, &long_option, opt_libtrace},
		{NULL,       0,                 NULL,         0}
	};

	s_progname = basename(argv[0]);
	while ((short_option = getopt_long(argc, argv, "hvV", longopts, NULL)) != -1)
	{
		switch (short_option)
		{
			case 0:
				switch (long_option)
				{
					case opt_help:
						print_usage(stdout, 1);
						return 0;
					case opt_version:
						print_version();
						return 0;
					case opt_cmd:
						strncpy(s_cmd_line, optarg, sizeof(s_cmd_line));
						break;
					case opt_dev:
					{
						unsigned s = 0;
						sscanf(optarg, "%[^,],%u", s_dev, &s);
						if(s == 115200) { s_speed = DRBCC_BR_115200; }
						else if(s == 57600) { s_speed = DRBCC_BR_57600; }
						else if(s == 921600) { s_speed = DRBCC_BR_921600; }
						break;
					}
					case opt_trace:
					{
						unsigned t = 0;
						sscanf(optarg, "%u", &t);
#ifdef HAVE_LIBDRTRACE
						DRHIP_TRACE_SET_LVL(drbcctrace, DRHIP_TCAT_3, t);
#else
						set_tracelevel(t);
#endif
						break;
					}
					case opt_libtrace:
						sscanf(optarg, "%li", &libtracemask);
						break;
				}
				break;
			case 'h':
				print_usage(stdout, 1);
				return 0;
		}
	}
#ifdef HAVE_LIBDRTRACE
	TRACE(TL_DEBUG, "tracelevel: %u", DRHIP_TRACE_GET_LVL(drbcctrace, DRHIP_TCAT_3));
#else
#endif
	TRACE(TL_DEBUG, "libdrbcc tracemask: 0x%08lX", libtracemask);
	TRACE(TL_DEBUG, "device: %s (%u baud)", s_dev, speed_2_uint(s_speed));

	argv += optind;
	argc -= optind;

	CHECKCALL(TL_DEBUG, rc, drbcc_init, (libtracemask));
	if(rc == DRBCC_RC_NOERROR)
	{
		CHECKCALL(TL_DEBUG, rc, establish_connection, ());
	}
	CHECKCALL(TL_DEBUG, rc, drbcc_term, ());

	return rc;
}

/* Editor hints for emacs
 *
 * Local Variables:
 * mode:c
 * c-basic-offset:4
 * indent-tabs-mode:t
 * tab-width:4
 * End:
 *
 * NO CODE BELOW THIS! */
