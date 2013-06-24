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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/timeb.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <termios.h>

#include "drbcc_ll.h"
#include "drbcc.h"
#include "drbcc_trace.h"
#include "drbcc_utils.h"

#ifdef HAVE_LIBDRTRACE
#else
#include "drbcc_trace.h"
#endif

#define DRBCC_MAJOR_VERSION 0
#define DRBCC_MINOR_VERSION 1

extern int libdrbcc_initialized;

#define lo8(x) ((x) & 0xff)
#define hi8(x) ((x) >> 8)

static uint8_t bcd2bin(uint8_t data)
{
	return ((data >> 4) * 10 + (data & 0xF));
}

uint16_t libdrbcc_crc_ccitt_update(uint16_t crc, uint8_t data)
{
	data ^= lo8 (crc);
	data ^= data << 4;

	return ((((uint16_t)data << 8) | hi8 (crc)) ^ (uint8_t)(data >> 4)
		^ ((uint16_t)data << 3));
}

void libdrbcc_add_msg_prio(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg)
{
	DRBCC_MESSAGE_t *next;

	if (drbcc->prioQueue == NULL)
	{
		drbcc->prioQueue = msg;
	}
	else
	{
		next = drbcc->prioQueue;
		while (next->next != NULL)
		{
			next = next->next;
		}
		next->next = msg;
	}

	msg->next = NULL;
}

void libdrbcc_add_msg_sec(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg)
{
	DRBCC_MESSAGE_t *next;

	if (drbcc->secQueue == NULL)
	{
		drbcc->secQueue = msg;
	}
	else
	{
		next = drbcc->secQueue;
		while (next->next != NULL)
		{
			next = next->next;
		}
		next->next = msg;
	}

	msg->next = NULL;
}

static void libdrbcc_proc_ack_msg(DRBCC_t *drbcc)
{
	switch(drbcc->repeatMsg->msg[0] & ~TOGGLE_BITMASK)
	{
	case DRBCC_SYNC:
		{
			if(1 == drbcc->wait_for_first_sync_ack)
			{
				/* the first sync request was send without a session,
				   no session callback must be called
				*/
				drbcc->wait_for_first_sync_ack = 0;
				return;
			}
		} // NO break here! we have to close the session
	case DRBCC_REQ_DEBUG_SET:
	case DRBCC_REQ_HEARTBEAT:
	case DRBCC_REQ_SET_LED:
	case DRBCC_REQ_SHUTDOWN:
	case DRBCC_REQ_HD_EJECT:
		{
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	default: break;
	}
}

static DRBCC_RC_t libdrbcc_proc_msg(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg)
{
	CHECK_HANDLE(drbcc);

	switch(msg->msg[0] & ~TOGGLE_BITMASK)
	{
	case DRBCC_IND_RINGLOG_POS:
		{
			drbcc->curFileIndex = ((msg->msg[1] << 8) | msg->msg[2]);
			drbcc->logentry = 0;
			drbcc->logwrapflag = 0;
			if(msg->msg_len >= 5)
			{
				drbcc->logentry = msg->msg[3];
				drbcc->logwrapflag = msg->msg[4];
			}
			TRACE(DRBCC_TR_MSGS, "Received DRBCC_IND_RINGLOG_POS message %x", drbcc->curFileIndex);
			if (drbcc->state == DRBCC_STATE_USER)
			{
				if (drbcc->getpos_cb)
				{
					drbcc->getpos_cb(drbcc->context, drbcc->curFileIndex, drbcc->logentry,  drbcc->logwrapflag);
				}
				if ( (0 != drbcc->session) && drbcc->session_cb)
				{
					drbcc->session_cb(drbcc->context, drbcc->session, 1);
					drbcc->session = 0;
				}
			}
			else
			{
				libdrbcc_logpos_cb(drbcc);
			}
			break;
		}
	case DRBCC_IND_PUT_LOG:
		{
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	case DRBCC_ACK:
		if (drbcc->error_cb)
		{
			char s[] = "Received illegal DRBCC_ACK message";
			drbcc->error_cb(drbcc->context, s);
		}
		TRACE_WARN("Received illegal DRBCC_ACK message");
		break;
	case DRBCC_CMD_ILLEGAL:
		if (drbcc->error_cb)
		{
			char s[] = "Received illegal message content";
			drbcc->error_cb(drbcc->context, s);
		}
		TRACE_WARN("Received illegal message content");
		break;
	case DRBCC_SYNC_CMD_ERROR:
		if (drbcc->error_cb)
		{
			char s[] = "Received DRBCC_SYNC_CMD_ERROR message";
			drbcc->error_cb(drbcc->context, s);
		}
		TRACE_WARN("Received DRBCC_SYNC_CMD_ERROR message");
		break;
	case DRBCC_CMD_REQ_PROTOCOL_VERSION:
		{
			DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

			if (msg != NULL)
			{
				msg->msg_len = 4;
				msg->msg[0] = DRBCC_CMD_IND_PROTOCOL_VERSION;
				msg->msg[1] = DRBCC_MAJOR_VERSION;
				msg->msg[2] = DRBCC_MINOR_VERSION;
				msg->msg[2] = 0;

				libdrbcc_add_msg_prio(drbcc, msg);
			}
		}
		break;
	case DRBCC_CMD_IND_PROTOCOL_VERSION:
		if (msg->msg_len >= 4)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call protocol callback");
			if (drbcc->protocol_cb)
			{
				if (msg->msg_len > 4)
				{
					drbcc->protocol_cb(drbcc->context, msg->msg[1], msg->msg[2], msg->msg[3], (msg->msg_len - 4), &(msg->msg[4]));
				}
				else
				{
					drbcc->protocol_cb(drbcc->context, msg->msg[1], msg->msg[2], msg->msg[3], 0, NULL);
				}
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_CMD_IND_PROTOCOL_VERSION";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_EXTFLASH_ID:// <mid> <mid> <devid1> <devid2>
		if (msg->msg_len >= 4)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call flash id callback");
			if (drbcc->flash_id_cb)
			{
				drbcc->flash_id_cb(drbcc->context, msg->msg[1], msg->msg[2], msg->msg[3]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_EXTFLASH_ID";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_EXTFLASH_READ: // <mid> <adrh> <adrm> <adrl> <len of data> <data[0]> ... <data[len-1]>
		if (msg->msg_len >= 5 && msg->msg_len == msg->msg[4] + 5)
		{
			unsigned addr = (msg->msg[1] << 16) | (msg->msg[2] << 8) | msg->msg[3];
			TRACE(DRBCC_TR_MSGS,  "Try to call read flash callback");
			if (drbcc->read_flash_cb)
			{
				drbcc->read_flash_cb(drbcc->context, addr, msg->msg[4], &(msg->msg[5]));
				if (drbcc->flashLen)
				{
					// request rest
					libdrbcc_req_flash_read(drbcc, drbcc->flashAddr, drbcc->flashLen);
				}
				else
				{
					if (drbcc->session_cb)
					{
						drbcc->session_cb(drbcc->context, drbcc->session, 1);
						drbcc->session = 0;
					}
				}
			}
			else
			{
				libdrbcc_readflash_cb(drbcc, addr, msg->msg[4], &(msg->msg[5]));
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_EXTFLASH_READ";
				drbcc->error_cb(drbcc->context, s);
			}
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	case DRBCC_IND_EXTFLASH_WRITE_RESULT: // <mid> <adrh> <adrm> <adrl> <len> <result>
		if (msg->msg_len >= 6)
		{
			unsigned addr = (msg->msg[1] << 16) | (msg->msg[2] << 8) | msg->msg[3];
			TRACE(DRBCC_TR_MSGS,  "Try to call write flash callback");
			if (drbcc->write_flash_cb)
			{
				drbcc->write_flash_cb(drbcc->context, addr, msg->msg[4], msg->msg[5]);
				if (drbcc->flashLen)
				{
					// request rest
					libdrbcc_flash_write(drbcc);
				}
				else
				{
					if (drbcc->session_cb)
					{
						if (drbcc->flashData)
						{
							free(drbcc->flashData);
							drbcc->flashData = NULL;
						}
						drbcc->session_cb(drbcc->context, drbcc->session, 1);
						drbcc->session = 0;
					}
				}
			}
			else
			{
				libdrbcc_writeflash_cb(drbcc, addr, msg->msg[4], msg->msg[5]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_EXTFLASH_WRITE_RESULT";
				drbcc->error_cb(drbcc->context, s);
			}
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	case DRBCC_IND_EXTFLASH_BLOCKERASE_RESULT: // <mid> <blkh> <blkl> <result>
		if (msg->msg_len >= 4)
		{
			TRACE(DRBCC_TR_MSGS,  "Try to call erase flash callback");
			if (drbcc->erase_flash_cb)
			{
				unsigned block = (msg->msg[1] << 8) | msg->msg[2];
				drbcc->erase_flash_cb(drbcc->context, block, msg->msg[3]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_EXTFLASH_BLOCKERASE_RESULT";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((DRBCC_STATE_USER == drbcc->state) && (drbcc->session_cb))
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_RTC_READ: //<mid> <sec> <min> <hour> <day> <date> <month> <year> <epoch>
		{
			struct tm t;
			if (msg->msg_len >= 9)
			{
				TRACE(DRBCC_TR_MSGS,  "Try to call rtc callback");
				if (drbcc->rtc_cb)
				{
					t.tm_sec	= bcd2bin(msg->msg[1]);
					t.tm_min	= bcd2bin(msg->msg[2]);
					t.tm_hour	= bcd2bin(msg->msg[3] & 0x3f);		// Bit 6 is 12/24h clock mode
					t.tm_wday	= bcd2bin(msg->msg[4]) - 1;			// rtc 1-7, tm 0-6
					t.tm_mday	= bcd2bin(msg->msg[5]);
					t.tm_mon	= bcd2bin(msg->msg[6] & 0x1f) - 1;	// MSB is century bit, rtc 1-12, tm 0-11
					t.tm_year	= bcd2bin(msg->msg[7]) + 100;		// tm is 1900
					if (msg->msg[6] & 0x80) // century bit
					{
						t.tm_year += 100;
					}
					t.tm_isdst = 0;
					t.tm_yday =  0;

					drbcc->rtc_cb(drbcc->context, &t, msg->msg[8]);
				}
			}
			else
			{
				if (drbcc->error_cb)
				{
					char s[] = "Received too short message content in DRBCC_IND_RTC_READ";
					drbcc->error_cb(drbcc->context, s);
				}
			}
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	case DRBCC_IND_FW_INVALIDATED:
		if (drbcc->error_cb)
		{
			char s[] = "BCTRL firmware successfully invalidated";
			drbcc->error_cb(drbcc->context, s);
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_BCTRL_RESTART_ACCEPTED:
		if (drbcc->error_cb)
		{
			char s[] = "BCTRL restart successfully initiated";
			drbcc->error_cb(drbcc->context, s);
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_FW_UPDATE_STARTED:
		if (drbcc->error_cb)
		{
			char s[] = "BCTRL firmware update successfully started";
			drbcc->error_cb(drbcc->context, s);
		}
		break;
	case DRBCC_IND_BL_UPDATE:
		if (drbcc->error_cb)
		{
			if (1 == msg->msg[1])
			{
				char s[] = "BCTRL Boot loader update successfully";
				drbcc->error_cb(drbcc->context, s);
			}
			else
			{
				char s[] = "BCTRL Boot loader update FAILED";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	case DRBCC_IND_ID_DATA:
		if (drbcc->id_cb && (3 < msg->msg_len))
		{
			drbcc->id_cb(drbcc->context, msg->msg[1], msg->msg[2], msg->msg_len - 3, &msg->msg[3]);
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "ID Data no callback or invalid payload";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
		// BCTRL requests HDD turn off (due to key eject request); OXE shall unmount HDD, then turn off HDD power
		// reason: 1 byte reserved for indicating eject reason; currently 0
	case DRBCC_HDD_OFF_REQ:	// <mid> <reason> <16 byte key token data>
		if (17 < msg->msg_len)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call HDOffRequest callback");
			if (drbcc->hdOffRequest_cb)
			{
				drbcc->hdOffRequest_cb(drbcc->context, msg->msg_len - 1, &msg->msg[1]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_HDD_OFF_REQ";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		break;
	case DRBCC_IND_STATUS:
		if (msg->msg_len > 1)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call status callback");
			if (drbcc->status_cb)
			{
				drbcc->status_cb(drbcc->context, msg->msg_len - 1, &msg->msg[1]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_STATUS";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		// if wait for ack true than this status indication is an unsolicited message from the BCTL
		// wait of the status request answer 
		// we have to close the session also if we wait for an ack and this may be unsolicited
		if (!drbcc->wait_for_ack || drbcc->indClosesSession)
		{
			drbcc->indClosesSession = 0;
			if ((0 != drbcc->session) && drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
				drbcc->session = 0;
			}
		}
		break;
	case DRBCC_IND_ACCEL_EVENT:
		if (msg->msg_len > 7)
		{
			int16_t xx = msg->msg[2] | (msg->msg[3] << 8) ;
			int16_t yy = msg->msg[4] | (msg->msg[5] << 8) ;
			int16_t zz = msg->msg[6] | (msg->msg[7] << 8);
			TRACE(DRBCC_TR_MSGS, "Try to call accel event callback");
			if (drbcc->accel_event_cb)
			{
				drbcc->accel_event_cb(drbcc->context, msg->msg[1], xx, yy, zz);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_ACCEL_EVENT";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		break;
	case DRBCC_IND_DEBUG_GET:  // <mid> <adrh> <adrl> <len> <data...>
		TRACE(DRBCC_TR_MSGS, "Try to call status callback");
		if(msg->msg_len >= 4)
		{
			if (drbcc->debug_get_cb)
			{
				uint32_t addr = ((msg->msg[1] << 8) | (msg->msg[2]));
				int len  = msg->msg[3];
				drbcc->debug_get_cb(drbcc->context, addr, len, &msg->msg[4]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_DEBUG_GET";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		if ((0 != drbcc->session) && drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
			drbcc->session = 0;
		}
		break;
	/*case DRBCC_IND_GPO:
		if (msg->msg_len >= 3)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call gpo callback");
			if (drbcc->gpo_cb)
			{
				drbcc->gpo_cb(drbcc->context, msg->msg[1], msg->msg[2]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_GPO";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		break;
	case DRBCC_IND_GPI:
		if (msg->msg_len >= 3)
		{
			TRACE(DRBCC_TR_MSGS, "Try to call protocol callback");
			if (drbcc->status_cb)
			{
				drbcc->status_cb(drbcc->context, msg->msg_len - 1, &msg->msg[2]);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				char s[] = "Received too short message content in DRBCC_IND_GPI";
				drbcc->error_cb(drbcc->context, s);
			}
		}
		break;*/
	default:
		if (drbcc->error_cb)
		{
			char s[512];
			snprintf(s, sizeof(s), "Unknown message ID 0x%X received", msg->msg[0] & ~TOGGLE_BITMASK);
			s[sizeof(s)-1] = '\0';
			drbcc->error_cb(drbcc->context, s);
		}
		break;
	break;
	}

	return DRBCC_RC_NOERROR;
}

void libdrbcc_free_queue(DRBCC_MESSAGE_t *msg)
{
	if (msg != NULL)
	{
		if (msg->next != NULL)
		{
			libdrbcc_free_queue(msg->next);
		}
		free(msg);
	}
}

// takes one byte of payload and does NO escaping
static DRBCC_RC_t libdrbcc_usart_send_byte(DRBCC_t *drbcc, int fd, uint8_t b, int flush)
{
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	TRACE(DRBCC_TR_TRANS, ">%02X>", b);

	drbcc->writebuf[drbcc->writepos++] = b;

	if (flush)
	{
		int res = write(fd, &(drbcc->writebuf), drbcc->writepos);
		if(res != drbcc->writepos)
		{
			TRACE_WARN("write() failed with %d (bytes to write %d) %.512s", res, drbcc->writepos, (res == -1) ? strerror(errno) : "");
			rc = DRBCC_RC_SYSTEM_ERROR;
		}
		drbcc->writepos = 0;
	}
	return rc;
}

// takes one byte of payload and does the escaping
static DRBCC_RC_t libdrbcc_usart_send_esc_byte(DRBCC_t *drbcc, int fd, uint8_t b)
{
	if (DRBCC_START_CHAR == b || DRBCC_STOP_CHAR == b || DRBCC_ESC_CHAR == b)
	{
		libdrbcc_usart_send_byte(drbcc, fd, DRBCC_ESC_CHAR, 0);
		b = ~b;
	}
	return libdrbcc_usart_send_byte(drbcc, fd, b, 0);
}

static void libdrbcc_send_ack(DRBCC_t *drbcc, DRBCC_MESSAGE_t *response)
{
	DRBCC_MESSAGE_t ack_msg;
	DRBCC_RC_t rc;

	ack_msg.msg_len = 1;

	// send ack with corresponding toggle
	if ((response->msg[0] & TOGGLE_BITMASK) == TOGGLE_BITMASK)
	{
		ack_msg.msg[0] = DRBCC_ACK | TOGGLE_BITMASK;
	}
	else
	{
		ack_msg.msg[0] = DRBCC_ACK;
	}

	TRACE(DRBCC_TR_TRANS, "Sending ACK...");
	if ((rc = libdrbcc_send_message(drbcc, &ack_msg)) != DRBCC_RC_NOERROR)
	{
		// if sending ack failed, request will be repeated and another ack will be sended
		TRACE_WARN("Sending ACK failed");
	}
}

DRBCC_RC_t libdrbcc_send_message(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg)
{
	DRBCC_RC_t rc;
	uint16_t crc = 0xffff;
	const uint8_t *pu;
	uint8_t idx;
	struct timeval now;

	libdrbcc_usart_send_byte(drbcc, drbcc->fd, DRBCC_START_CHAR, 0);

	if (msg->msg_len > 0)
	{
		if ((msg->msg[0] & ~TOGGLE_BITMASK) != DRBCC_ACK) // do not touch ack msgs, they already have valid toggle bit
		{
			if(drbcc->send_toggle)
			{
				msg->msg[0] |= TOGGLE_BITMASK;
			}
		}

		if ((msg->msg[0] & ~TOGGLE_BITMASK) == DRBCC_SYNC) //
		{
			drbcc->send_toggle = -1;
			drbcc->expected_recv_toggle = 0;
			msg->msg[0] |= TOGGLE_BITMASK;

			TRACE(DRBCC_TR_TRANS, "Set send_toggle to %x", drbcc->send_toggle);
		}

		for (pu = msg->msg, idx = msg->msg_len; idx > 0; idx--)
		{
			libdrbcc_usart_send_esc_byte(drbcc, drbcc->fd, *pu);
			crc = libdrbcc_crc_ccitt_update(crc, *pu++);
		}

		// send CRC
		libdrbcc_usart_send_esc_byte(drbcc, drbcc->fd, crc & 0xff);
		libdrbcc_usart_send_esc_byte(drbcc, drbcc->fd, crc >> 8);
	}

	rc = libdrbcc_usart_send_byte(drbcc, drbcc->fd, DRBCC_STOP_CHAR, 1);

	//tcdrain(drbcc->fd);

	if ((msg->msg[0] & ~TOGGLE_BITMASK) != DRBCC_ACK) // do not touch ack msgs, they already have valid toggle bit
	{
		gettimeofday(&now, NULL);

		timeradd(&now, &drbcc->ackTimeout, &drbcc->resend);
		timeradd(&now, &drbcc->nextTimeout, &drbcc->sendnext);
	}

	return rc;
}

// rx t-bit: this is the expected t-bit value for the next rx message
// clear on sync (msg: set t-bit to 0,
// on rx of message (crc correct): always send ack with t-bit of message
// if t-bit of message was correct (= expected t-bit): process message, toggle t-bit
// else discard message
// tx t-bit: this is the t-bit of the next message we will compose + send
// clear on sync  (msg: set t-bit to 0, ignore t-bit of the sync msg)
// send message with current t-bit, keep message for repeat, start ack timer
// on rx of ack with current t-bit: release message buffer, toggle t-bit
// on rx of ack with wrong t-bit: resend message
// on rx timeout for any ack: resend message

static DRBCC_RC_t libdrbcc_receive(DRBCC_t *drbcc)
{
	int rc = 0;
	uint8_t date;
	uint8_t ack = DRBCC_ACK;

	//gettimeofday(&now2, NULL);

	if (drbcc->bufend == drbcc->bufstart)
	{
		drbcc->bufstart = 0;
		drbcc->bufend = 0;
		rc = read(drbcc->fd, &(drbcc->readbuf[0]), sizeof(drbcc->readbuf));
	}
	else if (drbcc->bufend > 0 && drbcc->bufend < sizeof(drbcc->readbuf))
	{
		rc = read(drbcc->fd, &(drbcc->readbuf[drbcc->bufend]), sizeof(drbcc->readbuf) - drbcc->bufend);
	}

	if (rc > 0)
	{
		drbcc->bufend += rc;
	}

	while (drbcc->bufstart < drbcc->bufend)
	{
		date = drbcc->readbuf[drbcc->bufstart++];

		if (date == DRBCC_ESC_CHAR)
		{
			drbcc->escaped = 1;
			continue;
		}
		if (drbcc->escaped)
		{
			drbcc->escaped = 0,
			date = ~date;
		}
		else if (date == DRBCC_START_CHAR)
		{
			drbcc->cur_msg.msg_len = 0;
			drbcc->crc = 0xffff;
			drbcc->wait_for_stop_char = 1;
			TRACE(DRBCC_TR_TRANS, "<%02X<", date);

			continue;
		}
		else if (date == DRBCC_STOP_CHAR)
		{
			TRACE(DRBCC_TR_TRANS, "<%02X<", date);
			if(1 != drbcc->wait_for_stop_char)
			{
				TRACE_WARN("Unexpected stop char");
				continue;	
			}
			drbcc->wait_for_stop_char = 0;
			if(3 > drbcc->cur_msg.msg_len)
			{
				TRACE_WARN("received msg to short (%d byte(s))", drbcc->cur_msg.msg_len);
				continue;	
			}
			if (drbcc->crc == 0)
			{
				TRACE(DRBCC_TR_TRANS, "CRC OK");
				// Msg complete

				if (drbcc->send_toggle) // what we expect is
				{
					ack = DRBCC_ACK | TOGGLE_BITMASK;
					TRACE(DRBCC_TR_TRANS, " expect %x", ack);
				}

				if (drbcc->cur_msg.msg[0] == ack)	// our ack
				{
					if (drbcc->repeatMsg)
					{
						TRACE(DRBCC_TR_TRANS, "ACK libdrbcc_received");
						libdrbcc_proc_ack_msg(drbcc);
						drbcc->cur_msg.msg_len = 0;
						drbcc->cur_msg.msg[0] = DRBCC_CMD_ILLEGAL;
						drbcc->send_toggle = ~(drbcc->send_toggle);
						drbcc->wait_for_ack = 0;
						drbcc->repeatCount = 0;
						free (drbcc->repeatMsg);
						drbcc->repeatMsg = 0;
					} else {
						char s[] = "Received unexpected ack message";
						TRACE_WARN("%s", s);
						if (drbcc->error_cb) { drbcc->error_cb(drbcc->context, s); }
					}
					continue;
				}
				else if (drbcc->cur_msg.msg[0] == DRBCC_SYNC_ANSWER)
				{
					TRACE(DRBCC_TR_TRANS, "DRBCC_SYNC_ANSWER libdrbcc_received");
					drbcc->cur_msg.msg_len = 0;
					drbcc->cur_msg.msg[0] = DRBCC_CMD_ILLEGAL;
					drbcc->send_toggle = ~(drbcc->send_toggle);
					drbcc->wait_for_ack = 0;
					drbcc->repeatCount = 0;
					if (drbcc->repeatMsg) { free (drbcc->repeatMsg); }
					drbcc->repeatMsg = 0;
					drbcc->sync_mode = 1;
					drbcc->ackTimeout.tv_usec = 250*1000; // 100 ms in synch mode
					continue;
				}
				else if ((drbcc->cur_msg.msg[0] &~TOGGLE_BITMASK) != DRBCC_ACK) // another msg from peer
				{
					drbcc->cur_msg.msg_len -= 2;
					if (!(drbcc->sync_mode))
					{
						libdrbcc_send_ack(drbcc, &(drbcc->cur_msg));
						//gettimeofday(&now1, NULL);
						//fprintf(stderr, " ACK delay %i\n", now1.tv_usec - now2.tv_usec);
					}
					// process msg if toggle bit is correct or in sync mode
					if (drbcc->sync_mode || (drbcc->cur_msg.msg[0] & TOGGLE_BITMASK) == (drbcc->expected_recv_toggle & TOGGLE_BITMASK))
					{
						// toggle toggle bit
						drbcc->expected_recv_toggle = ~(drbcc->expected_recv_toggle);
						if (!drbcc->wait_for_ack)
						{
							drbcc->wait_for_answer = 0;
						}
						//else {
							// got an unsolicited message from the BCTL
							// wait of the request answer
						// }
						// call callbacks
						libdrbcc_proc_msg(drbcc, &(drbcc->cur_msg));
					}
					else
					{
						TRACE(DRBCC_TR_TRANS, "TOGGLE_BIT ERROR:");
						if (drbcc->error_cb)
						{
							char s[] = "TOGGLE_BIT ERROR";
							drbcc->error_cb(drbcc->context, s);
						}
						if ((0 != drbcc->session) && drbcc->session_cb)
						{
							drbcc->session_cb(drbcc->context, drbcc->session, 0);
							drbcc->session = 0;
						}
						drbcc->state = DRBCC_STATE_USER;
					}
					if (drbcc->sync_mode)
					{
						TRACE(DRBCC_TR_TRANS, "msg in sync mode received");
						drbcc->send_toggle = 0;
						drbcc->wait_for_ack = 0;
						drbcc->wait_for_answer = 0;
						drbcc->repeatCount = 0;
						if (drbcc->repeatMsg)
						{
							free(drbcc->repeatMsg);
						}
						drbcc->repeatMsg = 0;
						continue;
					}
				}
				else
				{
					TRACE_WARN("libdrbcc_received %x expected %x", drbcc->cur_msg.msg[0], ack);
				}
			}
			else
			{
				TRACE_WARN("CRC error");
			}
			return DRBCC_RC_NOERROR;
		}

		drbcc->crc = libdrbcc_crc_ccitt_update(drbcc->crc, date);
		drbcc->cur_msg.msg[drbcc->cur_msg.msg_len++] = date;
		TRACE(DRBCC_TR_TRANS, "<%02X<", date);
	}
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_trigger(DRBCC_HANDLE_t h, int maxLoops)
{
	int i;
	DRBCC_MESSAGE_t* msg;
	DRBCC_RC_t rc = DRBCC_RC_NOERROR;
	DRBCC_t* drbcc = (DRBCC_t*)h;
	struct timeval now;

	CHECK_HANDLE(drbcc);

	for (i = maxLoops; i != 0; i--)
	{
		libdrbcc_receive(drbcc);

		if (!drbcc->wait_for_ack && drbcc->prioQueue)
		{
			// get msg
			msg = drbcc->prioQueue;
			// process queue
			drbcc->prioQueue = msg->next;
			TRACE(DRBCC_TR_QUEUE, "Send request 1st queue:");
			drbcc->repeatMsg = msg;
			libdrbcc_send_message(drbcc, msg);
			drbcc->wait_for_ack = 1;
		}
		else if (!drbcc->wait_for_ack && !drbcc->wait_for_answer && drbcc->secQueue)
		{
			// get msg
			msg = drbcc->secQueue;
			// process queue
			drbcc->secQueue = msg->next;
			TRACE(DRBCC_TR_QUEUE, "Send request 2nd queue:");
			libdrbcc_send_message(drbcc, msg);
			drbcc->repeatMsg = msg;
			drbcc->wait_for_ack = 1;
			drbcc->wait_for_answer = 1;
		}

		gettimeofday(&now, NULL);

		if (drbcc->wait_for_answer && timercmp(&now, &(drbcc->sendnext), >)) // sendnext expired
		{
			drbcc->wait_for_answer = 0; // stop waiting
		}

		if (drbcc->wait_for_ack && timercmp(&now, &(drbcc->resend), >))
		{
			if (drbcc->repeatCount < 25)
			{
				if (drbcc->repeatMsg)
				{
					TRACE(DRBCC_TR_QUEUE, "repeat sending msg cause ACK timeout %i", drbcc->repeatCount);
					if (drbcc->error_cb)
					{
						char s[] = "REPEAT sending msg cause ACK timeout";
						drbcc->error_cb(drbcc->context, s);
					}
					libdrbcc_send_message(drbcc, drbcc->repeatMsg);
					timeradd(&now, &drbcc->ackTimeout, &drbcc->resend);
					drbcc->repeatCount++;
				}
			}
			else
			{
				TRACE(DRBCC_TR_QUEUE, "ERROR: max repeat counter %i reached, sending msg failed", drbcc->repeatCount);
				if (drbcc->error_cb)
				{
					char s[] = "ERROR: Sending failed after repeat counter reached maximum";
					drbcc->error_cb(drbcc->context, s);
				}
				if ((0 != drbcc->session) && drbcc->session_cb)
				{
					drbcc->session_cb(drbcc->context, drbcc->session, 1);
					drbcc->session = 0;
				}
				drbcc->wait_for_ack = 0;
				drbcc->repeatCount = 0;
				if (drbcc->repeatMsg) { free(drbcc->repeatMsg); }
				drbcc->repeatMsg = 0;
			}
		}

		if (!drbcc->wait_for_ack && !drbcc->wait_for_answer && !drbcc->secQueue && !drbcc->prioQueue)
		{
			// nothing to do
			break;
		}
	}

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
