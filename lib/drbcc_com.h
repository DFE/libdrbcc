/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/*
 * Copyright (C) 2013 DResearch Fahrzeugelektronik GmbH
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as 
 * published by the Free Software Foundation, either version 3 of the 
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DRBCC_COM_
#define _DRBCC_COM_

/*/////////////////////////////////////////////////////////////////////*/
/*                                                                     */
/* code compatibility definitions (HOST was formerly known as OXE)     */
/*                                                                     */
/* do not use in new code!                                             */
/*                                                                     */
/*/////////////////////////////////////////////////////////////////////*/
#define DRBCC_E_OXE_INT1_ON                  DRBCC_E_HDD_USABLE_ON
#define DRBCC_PWR_FAILURE_OXE                DRBCC_PWR_FAILURE_HOST
#define DRBCC_SFI_OXE_INT1_bp                DRBCC_SFI_HDD_USABLE_bp
#define DRBCC_SFI_OXE_INT2_bp                DRBCC_SFI_HOST_RUNNING_bp
#define DRBCC_SFI_OXE_INT1_bm                DRBCC_SFI_HDD_USABLE_bm
#define DRBCC_SFI_OXE_INT2_bm                DRBCC_SFI_HOST_RUNNING_bm
#define DRBCC_DEBUG_SET_ADR_OXE_RESET        DRBCC_DEBUG_SET_ADR_HOST_RESET
#define DRBCC_DEBUG_SET_ADR_OXE_INT1_ONOFF   DRBCC_DEBUG_SET_ADR_HDD_USABLE_ONOFF
/* end of code compatibility definitions */


typedef enum
{
	DRBCC_RC_NOERROR = 0,
	DRBCC_RC_SYSTEM_ERROR,
	DRBCC_RC_UNSPEC_ERROR,
	DRBCC_RC_NO_STANDBY_POWER,
	DRBCC_RC_MISSING_START_CHAR,
	DRBCC_RC_MSG_TOO_LONG,
	DRBCC_RC_UNEXP_START_CHAR,
	DRBCC_RC_MSG_TOO_SHORT,
	DRBCC_RC_MSG_CRC_ERROR,
	DRBCC_RC_DEVICE_LOCKED,
	DRBCC_RC_MSG_TIMEOUT,
	DRBCC_RC_OUTOFMEMORY,
	DRBCC_RC_WRONGSTATE,
	DRBCC_RC_NOTINITIALIZED,
	DRBCC_RC_INVALID_HANDLE,
	DRBCC_RC_CBREGISTERED,
	DRBCC_RC_INVALID_FILENAME,
	DRBCC_RC_SESSIONACTIVE,
} DRBCC_RC_t;

// supported serial baud rates
typedef enum
{
	DRBCC_BR_57600,
	DRBCC_BR_115200,
	DRBCC_BR_921600
} DRBCC_BR_t;

typedef enum
{
	DRBCC_FLASHFILE_T_FW_IMAGE		= 0x0,
	DRBCC_FLASHFILE_T_BOOTLOADER	= 0x1,
	DRBCC_FLASHFILE_T_UBOOT_IMG		= 0x2,
	DRBCC_FLASHFILE_T_UBOOT_ENV		= 0x3,
	DRBCC_FLASHFILE_T_BOARDTEST  	= 0x4,
	DRBCC_FLASHFILE_T_DEVID         = 0x5, /* device identity info */
	DRBCC_FLASHFILE_T_RESERVED7		= 0x6,
	DRBCC_FLASHFILE_T_RESERVED8		= 0x7,
} DRBCC_FLASHFILE_TYPES_t;

typedef enum
{
	DRBCC_FLASHBLOCK_T_FREE			= 0x7,
	DRBCC_FLASHBLOCK_T_RING_LOG		= 0x6,
	DRBCC_FLASHBLOCK_T_PERS_LOG		= 0x5,
	DRBCC_FLASHBLOCK_T_RESERVED0	= 0x4,
	DRBCC_FLASHBLOCK_T_RESERVED1	= 0x3,
	DRBCC_FLASHBLOCK_T_RESERVED2	= 0x2,
	DRBCC_FLASHBLOCK_T_RESERVED3	= 0x1,
	DRBCC_FLASHBLOCK_T_RESERVED4	= 0x0,
} DRBCC_FLASHBLOCK_TYPES_t;

// log event codes
typedef enum
{
	DRBCC_E_EXTENSION = 1,    // indicates that this log entry is a data extension of a previous log entry, it contains no timestamp
	DRBCC_E_RAMLOG_OVERRUN,   //  overrun occured in RAM logging
	DRBCC_E_ILL_BID,          // illegal board revision ID detected
	DRBCC_E_ILL_PWR_STATE,    // illegal power state
	DRBCC_E_PWR_LOSS,         // power loss
	DRBCC_E_HOST_LOGENTRY,    // entry made by host
	DRBCC_E_PWR_CHNG,         // power state changed
	DRBCC_E_ILL_INT,          // unknown interrupt occured (woke us from power down)
	DRBCC_E_HDD_CHNG,         // state of HDD sensor changed
	DRBCC_E_KEY_DETECTED,     // key was detected, param: key eeprom code (7 byte)
	DRBCC_E_KEY_REJECTED,     // key was rejected, no applicable token was found, param: number of tokens (1 byte)
	DRBCC_E_KEY_SUCCESS,      // key was successfully processed, param: begin of token (6 byte) + number of eject retries (1 byte)
	DRBCC_E_UNLOCK_ERROR,     // hdd unlock failed, param: begin of token (6 byte) + number of eject retries (1 byte)
	DRBCC_E_KEY_COMM_ERROR,   // key handling failed: invalid key eeprom code,  param: key eeprom code (7 byte)
	DRBCC_E_KEY_HEADER_ERROR, // key handling failed: error in key header data, param: begin of key header data (4 byte)
	DRBCC_E_RTC_SET,          // RTC willbe set to new time (caused epoch change); timestamp is old time, param: new time
	DRBCC_E_COMM_TIMEOUT,     // HOST Communication Heartbeat timeout, Param: 0=first message, 1=heartbeat, 2=shutdown
	DRBCC_E_VOLTAGE_INFO,     // List of internal voltage values; param: one or more pairs of voltage ID (1 byte enum) and value in 10mV (2 byte int signed)
	DRBCC_E_LOG_CLEARED,      // Ringlog was cleared
	DRBCC_E_HDD_USABLE_ON,    // HDD_USABLE signal line was turned on (HD may now be used)
	DRBCC_E_FW_UPDATE,        // firmware update performed , Param: result: 0=error 1=ok
	DRBCC_E_BL_UPDATE,        // bootloader update performed, Param: result: 0=error 1=ok
	DRBCC_E_FW_REBOOT,        // firmware reboot (e.g. in order to do a fw update) initiated, Param: 1= with 0=without SRAM context reset
	DRBCC_E_OVERTEMP_OFF,     // emergency turn off: hard high temperature limit exceeded, Param: temp (int8)
	DRBCC_E_TEMPLIMIT,        // could not turn on because of temp outside safe limits, Param: temp , lower limit, upper limit, reset limit (alle int8)
	DRBCC_E_ACCEL_EVENT,      // acceleration sensor event, Params: Event type (byte), accel data (6 byte: xx, yy, zz)
	DRBCC_E_EMPTY = 0xff      // event code 0xff indicates that this log entry is empty
} DRBCC_LOG_EVENT_t;

// voltage IDs
typedef enum
{
	DRBCC_VID_POWER_FILTER = 0, // power filter input voltage
	DRBCC_VID_POWER_CAP,        // power capacitor input voltage
	DRBCC_VID_PWR_CAM,          // camera power terminal voltage
	DRBCC_VID_VKEY,             // vkey step up supply voltage
	DRBCC_VID_SUPERCAP,         // HDD supply supercap voltage
	DRBCC_VID_12V,              // 12V camera power supply voltage
	DRBCC_VID_5V,               // 5V supply voltage
	DRBCC_VID_VDD,              // switched 3.3V supply voltage
	DRBCC_VID_1V8,              // 1.8V supply voltage
	DRBCC_VID_1V2,              // 1.2V supply voltage
	DRBCC_VID_1V,               // 1.0V supply voltage
	DRBCC_VID_3V3D,             // 3.3V BCTRL supply voltage
	DRBCC_VID_1V5,              // 1.5V supply voltage (new with hidav)
	DRBCC_VID_TERM,             // DRAM term power voltage (new with hidav)
	DRBCC_VID_VBAT,             // Li-Batt Voltage (remembered value from when last running on battery)
} DRBCC_VOLTAGE_ID_t;

// BCTRL power states
typedef enum
{
	DRBCC_P_MAIN_STATE_MASK			= 0x07, // mask for power state (without options)

	// main power states
	DRBCC_P_UNKNOWN					= 0,    // power state unknown (just initialized after reset)
	DRBCC_P_LITHIUM_POWER			= 1,    // neither standby nor key power present (lithium powered)
	DRBCC_P_KEY_POWER				= 2,    // VKey activated, VKey-Bypass-FET closed (/VKEY_FET)
	DRBCC_P_STANDBY_POWER			= 3,    // StandBy voltage present, standby-bypass-FET closed (/STBY_FET)
	DRBCC_P_HOST_POWER				= 4,    // host application powered (PWR_EN)

	// these are the options that can be combined with the main power state
	DRBCC_P_OPT_KEY_POWER_ENABLED	= 0x80, // VKey activated (VKEY_PWR_EN)
	DRBCC_P_OPT_EXTENDED_PWR		= 0x40, // additional chips activated (/3V3S_EN)
	DRBCC_P_OPT_DCDC_PWR			= 0x20, // Recom DC-DC converter activated (/DCDC_EN)
	DRBCC_P_OPT_HDD_PWR				= 0x10, // HDD activated (HDD_PWR_EN)
	DRBCC_P_OPT_LOCK_CHG			= 0x08, // enable the HDD lock charger (also necessary for P_KEY_POWER and P_OPT_KEY_POWER_ENABLED)
} DRBCC_Power_State_t;

// power loss reasons
typedef enum
{
	DRBCC_PWR_LOSS_VKEY_TOOLOW,
	DRBCC_PWR_LOSS_VKEY_EJCT_LOW,
	DRBCC_PWR_LOSS_VKEY_LOCK,
	DRBCC_PWR_LOSS_VKEY_RUN,
	DRBCC_PWR_FAILURE_HOST,
	DRBCC_PWR_LOSS_MAIN,
	DRBCC_PWR_LOSS_PREALERT,
	DRBCC_PWR_LOSS_SUPERCAP,
} DRBCC_Power_Loss_t;

// power loss reasons
typedef enum
{
	DRBCC_TIMEOUT_FIRST_MESSAGE,
	DRBCC_TIMEOUT_HEARTBEAT,
	DRBCC_TIMEOUT_SHUTDOWN
} DRBCC_Timeouts_t;

// accel event types
typedef enum
{
	DRBCC_ACCEL_EVENT_THRESHOLD_HIGH = 1,
} DRBCC_Accel_Events_t;

// bitposition definitions for key event action request code
#define DRBCC_KEYEVENT_REQ_MASK_NONE    0x00
#define	DRBCC_KEYEVENT_REQ_MASK_HDOFF   0x01

// bitposition definitions for key token command byte 0
#define DRBCC_KEY_CMD0_MASK_EJECT     0x01
#define DRBCC_KEY_CMD0_MASK_CLEAR     0x02
#define DRBCC_KEY_CMD0_MASK_ADAPT     0x04
#define DRBCC_KEY_CMD0_MASK_NOCOMP    0x08
#define DRBCC_KEY_CMD0_MASK_EXPDATE   0x10

// special value used to indicate TTU mode eject event (without actual key)
// log parser should display TTU EJECT instead of key event, and ignore the rest of keydata
#define DRBCC_KEY_CMD0_TTU_EJECT      0xE9

// bitposition/bitmask definitions for IO status data
// special function inputs (value & delta bits)
#define DRBCC_SFI_IGNITION_bp       0
#define DRBCC_SFI_HDD_SENS_bp       1
#define DRBCC_SFI_HDD_USABLE_bp     2
#define DRBCC_SFI_HOST_RUNNING_bp   3
#define DRBCC_SFI_XOSC_ERR_bp       4
#define DRBCC_SFI_TTU_HDD_UNLOCK_bp 5
#define DRBCC_SFI_IGNITION_bm       (1 << DRBCC_SFI_IGNITION_bp)
#define DRBCC_SFI_HDD_SENS_bm       (1 << DRBCC_SFI_HDD_SENS_bp)
#define DRBCC_SFI_HDD_USABLE_bm     (1 << DRBCC_SFI_HDD_USABLE_bp)
#define DRBCC_SFI_HOST_RUNNING_bm   (1 << DRBCC_SFI_HOST_RUNNING_bp)
#define DRBCC_SFI_XOSC_ERR_bm       (1 << DRBCC_SFI_XOSC_ERR_bp)
#define DRBCC_SFI_TTU_HDD_UNLOCK_bm (1 << DRBCC_SFI_TTU_HDD_UNLOCK_bp)

// general purpose inputs (value & delta bits)
#define DRBCC_GPI_Input1_bp      0
#define DRBCC_GPI_Input2_bp      1
#define DRBCC_GPI_Input3_bp      2
#define DRBCC_GPI_Input4_bp      3
#define DRBCC_GPI_Input5_bp      4
#define DRBCC_GPI_Input6_bp      5
#define DRBCC_GPI_Input1_bm      (1 << DRBCC_GPI_Input1_bp)
#define DRBCC_GPI_Input2_bm      (1 << DRBCC_GPI_Input2_bp)
#define DRBCC_GPI_Input3_bm      (1 << DRBCC_GPI_Input3_bp)
#define DRBCC_GPI_Input4_bm      (1 << DRBCC_GPI_Input4_bp)
#define DRBCC_GPI_Input5_bm      (1 << DRBCC_GPI_Input5_bp)
#define DRBCC_GPI_Input6_bm      (1 << DRBCC_GPI_Input6_bp)

#define DRBCC_NUM_OF_GPIs        6

/** Get the BIT of a GPI on the given position range [1..DRBCC_NUM_OF_GPIs] */
#define DRBCC_GPI_BIT(pos, data) (((data) >> ((pos) - 1)) & 0x01)

/** Get the bitmask for a given GPIx with x in 1..DRBCC_NUM_OF_GPIs*/
#define DRBCC_GPI_MASK(pos) (1 << ((pos) - 1))


// special function outputs
#define DRBCC_SFO_GPIPWR_bp      0
#define DRBCC_SFO_HDDPWR_bp      1
#define DRBCC_SFO_GPIPWR_bm      (1 << DRBCC_SFO_GPIPWR_bp)
#define DRBCC_SFO_HDDPWR_bm      (1 << DRBCC_SFO_HDDPWR_bp)


// general purpose outputs
#define DRBCC_GPO_relay1_bp      0
#define DRBCC_GPO_relay2_bp      1
#define DRBCC_GPO_relay3_bp      2
#define DRBCC_GPO_relay4_bp      3
#define DRBCC_GPO_relay1_bm      (1 << DRBCC_GPO_relay1_bp)
#define DRBCC_GPO_relay2_bm      (1 << DRBCC_GPO_relay2_bp)
#define DRBCC_GPO_relay3_bm      (1 << DRBCC_GPO_relay3_bp)
#define DRBCC_GPO_relay4_bm      (1 << DRBCC_GPO_relay4_bp)

#define DRBCC_NUM_OF_GPOs        4

/** Get the BIT of a GPO on the given position range [1..DRBCC_NUM_OF_GPOs] */
#define DRBCC_GPO_BIT(pos, data) (((data) >> ((pos) - 1)) & 0x01)

#define DRBCC_NUM_OF_LEDs        4

// BCTRL ring log area (counted in 4k blocks, external flash)
#define DRBCC_LOG_FIRSTBLOCK     4
#define DRBCC_LOG_LASTBLOCK      0x1ff

// DRBCC_REQ_DEBUG_SET address definitions
typedef enum
{
	DRBCC_CONFIG_SET_ADR_TEMPLIMIT_POWER_LOW  = 0x0100,  // len=1, val: lower power temp limit, int8
	DRBCC_CONFIG_SET_ADR_TEMPLIMIT_POWER_HIGH = 0x0101,  // len=1, val: upper power temp limit, int8
	DRBCC_CONFIG_SET_ADR_TEMPLIMIT_RESET      = 0x0102,  // len=1, val: reset temp limit, int8
	DRBCC_CONFIG_SET_ADR_TEMPLIMIT_HARD_HIGH  = 0x0103,  // len=1, val: hard turn off high temp limit, int8
	DRBCC_CONFIG_SET_ADR_ACCEL_THRESHOLD_LOW  = 0x0104,  // len=4, val: accel event low threshold: (g-value * 256)^2, uint32, hibyte first
	DRBCC_CONFIG_SET_ADR_ACCEL_THRESHOLD_HIGH = 0x0105,  // len=4, val: accel event high threshold: (g-value * 256)^2, uint32, hibyte first
} DRBCC_DEBUG_SET_ADR_t;

// DRBCC_REQ_DEBUG_GET address definitions
typedef enum
{
	DRBCC_CONFIG_GET_ADR_TEMPLIMIT_POWER_LOW  = 0x0100,  // len=1, val: lower power temp limit, int8
	DRBCC_CONFIG_GET_ADR_TEMPLIMIT_POWER_HIGH = 0x0101,  // len=1, val: upper power temp limit, int8
	DRBCC_CONFIG_GET_ADR_TEMPLIMIT_RESET      = 0x0102,  // len=1, val: reset temp limit, int8
	DRBCC_CONFIG_GET_ADR_TEMPLIMIT_HARD_HIGH  = 0x0103,  // len=1, val: hard turn off high temp limit, int8
	DRBCC_CONFIG_GET_ADR_ACCEL_THRESHOLD_LOW  = 0x0104,  // len=4, val: accel event low threshold: (g-value * 256)^2, uint32, hibyte first
	DRBCC_CONFIG_GET_ADR_ACCEL_THRESHOLD_HIGH = 0x0105,  // len=4, val: accel event high threshold: (g-value * 256)^2, uint32, hibyte first
} DRBCC_DEBUG_GET_ADR_t;

#ifndef __AVR__

#include <time.h>

/* the CODEC calling convention */
#ifdef _WIN32
#define DRBCC_API	WINAPI
#else
#define DRBCC_API
#endif

typedef unsigned DRBCC_SESSION_t;


typedef struct
{
	union
	{
		struct
		{
		uint8_t
			idx		  : 4,	// index
			type	  : 3,	// type:  DRBCC_BLOCK_TYPES_t or DRBCC_BYTE_TYPES_t
			blocktype : 1;	// length in blocks
		}bits;
		uint8_t typeinfo;
	} type;

	uint16_t startblock; // LSB first, 4K-blocks, start at block 4

	uint32_t length; // in blocks or in bytes (number of used blocks: ((numbytes + 0xfff)/0x1000))
} DRBCC_PARTENTRY_t;

// Callbacks
typedef void (DRBCC_API *DRBCC_ERROR_CB_t)(void *context, char* msg);

typedef void (DRBCC_API *DRBCC_SESSION_CB_t)(void *context, DRBCC_SESSION_t session, int success);

typedef void (DRBCC_API *DRBCC_PROTOCOL_CB_t)(void *context, int major, int minor, int fw_running, unsigned len, uint8_t *build_info);

typedef void (DRBCC_API *DRBCC_ID_CB_t)(void *context, int boardID, int slotID, unsigned len, uint8_t *serial);

typedef void (DRBCC_API *DRBCC_STATUS_CB_t)(void *context, unsigned len, uint8_t *state);

typedef void (DRBCC_API *DRBCC_RTC_CB_t)(void *context, const struct tm* utctime, int epoch);

typedef void (DRBCC_API *DRBCC_FLASH_ID_CB_t)(void *context, uint8_t mid, uint8_t devid1, uint8_t devid2);

typedef void (DRBCC_API *DRBCC_READ_FLASH_CB_t)(void *context, unsigned addr, unsigned len, uint8_t* data);

typedef void (DRBCC_API *DRBCC_WRITE_FLASH_CB_t)(void *context, unsigned addr, unsigned len, uint8_t result);

typedef void (DRBCC_API *DRBCC_ERASE_FLASH_CB_t)(void *context, unsigned blocknum, uint8_t result);

typedef void (DRBCC_API *DRBCC_PARTITION_TABLE_CB_t)(void *context, unsigned num, DRBCC_PARTENTRY_t entries[]);

typedef void (DRBCC_API *DRBCC_PROGRESS_CB_t)(void *context, int cur, int max);

typedef void (DRBCC_API *DRBCC_GETLOG_CB_t)(void *context, int pos, int len, uint8_t data[]);

typedef void (DRBCC_API *DRBCC_GETPOS_CB_t)(void *context, int pos, uint8_t entry, uint8_t wrapflag);

typedef void (DRBCC_API *DRBCC_DEBUG_GET_CB_t)(void *context, uint32_t addr, int len, uint8_t data[]);

/** Called on acceleration events.
 *  \param context context pointer (see \ref drbcc_start).
 *  \param type acceleration event type (see \ref DRBCC_Accel_Events_t).
 *  \param x X-axis acceleration.
 *  \param y Y-axis acceleration.
 *  \param z Z-axis acceleration.
 * \note Values of parameter x, y and z: 1g is equivalent to value 256, value 1 is equivalent to 3,90625mg(1000/256).
 */
typedef void (DRBCC_API *DRBCC_ACCEL_EVENT_CB_t)(void *context, DRBCC_Accel_Events_t type, int16_t x, int16_t y, int16_t z);

#endif /* __AVR__ */


#endif /* _DRBCC_COM_ */

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
