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
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>

#include "drbcc.h"
#include "drbcc_ll.h"
#include "drbcc_trace.h"
#include "drbcc_utils.h"

#define RETRIES 10

extern int libdrbcc_initialized;
extern DRBCC_SESSION_t drbcc_session;

// define the payload length of messages used for bulk data transfers (flash programming)
#define CHUNK 128

#ifdef HAVE_LIBDRTRACE
#else
#include "drbcc_trace.h"
static unsigned long s_tracemask = 0;
unsigned long libdrbcc_get_tracemask()
 	{
 	    return s_tracemask;
 	}
 	
void libdrbcc_set_tracemask(unsigned long t)
 	{
 	    s_tracemask = t;
 	}
#endif

static uint8_t bin2bcd(uint8_t data)
{
	return (((data / 10) << 4) + (data % 10));
}

DRBCC_RC_t drbcc_session_activ(DRBCC_HANDLE_t h)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_sync(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_SYNC;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_req_protocol(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_CMD_REQ_PROTOCOL_VERSION;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}
DRBCC_RC_t drbcc_register_rtc_cb(DRBCC_HANDLE_t h, DRBCC_RTC_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->rtc_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_req_rtc(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_RTC_READ;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}



DRBCC_RC_t drbcc_set_rtc(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session,  const struct tm* utctime)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 8;
		msg->msg[0] = DRBCC_REQ_RTC_SET;
		msg->msg[1] =  bin2bcd(utctime->tm_sec);
		msg->msg[2] =  bin2bcd(utctime->tm_min);
		msg->msg[3] =  bin2bcd(utctime->tm_hour);
		msg->msg[4] =  bin2bcd(utctime->tm_wday + 1);	// rtc 1-7, tm 0-6
		msg->msg[5] =  bin2bcd(utctime->tm_mday);
		msg->msg[6] =  bin2bcd(utctime->tm_mon  + 1);
		msg->msg[7] =  bin2bcd(utctime->tm_year % 100);
		if (utctime->tm_year > 199)
		{
			msg->msg[6] = msg->msg[6] | 0x80; // Century bit
		}

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}

		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_register_error_cb(DRBCC_HANDLE_t h, DRBCC_ERROR_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->error_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_session_cb(DRBCC_HANDLE_t h, DRBCC_SESSION_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->session_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_protocol_cb(DRBCC_HANDLE_t h, DRBCC_PROTOCOL_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->protocol_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_id_cb(DRBCC_HANDLE_t h, DRBCC_ID_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->id_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_status_cb(DRBCC_HANDLE_t h, DRBCC_STATUS_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->status_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_accel_event_cb(DRBCC_HANDLE_t h, DRBCC_ACCEL_EVENT_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->accel_event_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_hdOffRequest_cb(DRBCC_HANDLE_t h, DRBCC_STATUS_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->hdOffRequest_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_flash_id_cb(DRBCC_HANDLE_t h, DRBCC_FLASH_ID_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->flash_id_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_flash_read_cb(DRBCC_HANDLE_t h, DRBCC_READ_FLASH_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->read_flash_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_flash_write_cb(DRBCC_HANDLE_t h, DRBCC_WRITE_FLASH_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->write_flash_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_flash_erase_cb(DRBCC_HANDLE_t h, DRBCC_ERASE_FLASH_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->erase_flash_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_heartbeat(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int timeout)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 3;
		msg->msg[0] = DRBCC_REQ_HEARTBEAT;
		msg->msg[1] = (uint8_t) ((timeout >> 8) & 0xFF);
		msg->msg[2] = (uint8_t) ((timeout >> 0) & 0xFF);

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_shutdown(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int timeout)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 3;
		msg->msg[0] = DRBCC_REQ_SHUTDOWN;
		msg->msg[1] = (uint8_t) ((timeout >> 8) & 0xFF);
		msg->msg[2] = (uint8_t) ((timeout >> 0) & 0xFF);

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_set_gpo(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session,  unsigned int gpo, unsigned int onoff)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 3;
		msg->msg[0] = DRBCC_REQ_SET_GPO;
		msg->msg[1] = (uint8_t) gpo;
		msg->msg[2] = (uint8_t) onoff;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
			drbcc->indClosesSession = 1;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_set_led(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int num, unsigned int color, unsigned int on_time, unsigned int off_time, unsigned int phase)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 3;
		msg->msg[0] = DRBCC_REQ_SET_LED;
		msg->msg[1] = (uint8_t) num;
		msg->msg[2] = (uint8_t) color;
		if ((on_time != 0) || (off_time != 0)) // extended parameters used
		{
			msg->msg_len = 6;
			msg->msg[3] = (uint8_t) on_time;
			msg->msg[4] = (uint8_t) off_time;
			msg->msg[5] = (uint8_t) phase;
		}

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}


DRBCC_RC_t drbcc_get_status(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_STATUS;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
			drbcc->indClosesSession = 1;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_get_id_data(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_ID_DATA;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_eject_hd(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_HD_EJECT;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_hd_power(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int onoff)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 2;
		msg->msg[0] = DRBCC_REQ_HD_ONOFF;
		msg->msg[1] = (onoff & 0xff);

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
			drbcc->indClosesSession = 1;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_gpi_power(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int onoff)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 2;
		msg->msg[0] = DRBCC_REQ_GPI_POWER;
		msg->msg[1] = (onoff & 0xff);

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
			drbcc->indClosesSession = 1;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_req_flash_id(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_EXTFLASH_ID;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t libdrbcc_req_flash_read(DRBCC_t *drbcc, unsigned addr, unsigned len)
{
	unsigned size;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		if (CHUNK <= len)
		{
			size = CHUNK;
		}
		else
		{
			size = (len % CHUNK);
		}
		msg->msg_len = 5;
		msg->msg[0] = DRBCC_REQ_EXTFLASH_READ;
		msg->msg[1] = (uint8_t) ((addr >> 16) & 0xFF);
		msg->msg[2] = (uint8_t) ((addr >>  8) & 0xFF);
		msg->msg[3] = (uint8_t) ((addr >>  0) & 0xFF);
		msg->msg[4] = (uint8_t) size;

		libdrbcc_add_msg_sec(drbcc, msg);

		drbcc->flashAddr = addr + size;
		drbcc->flashLen = len - size;
	}
	else
	{
		return DRBCC_RC_OUTOFMEMORY;
	}

	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_req_flash_read(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned addr, unsigned len)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}
	drbcc->state = DRBCC_STATE_USER;

	if((0 != session) && drbcc->session_cb)
	{
		drbcc->session = drbcc_session++;
		*session = drbcc->session;
	}

	return libdrbcc_req_flash_read(h, addr, len);
}

DRBCC_RC_t libdrbcc_flash_write(DRBCC_t *drbcc)
{
	unsigned int i;
	unsigned size;

	if (drbcc->flashLen >= CHUNK)
	{
		size =  CHUNK;
	}
	else
	{
		size = drbcc->flashLen;
	}
	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 5 + size;
		msg->msg[0] = DRBCC_REQ_EXTFLASH_WRITE; // <mid> <adrh> <adrm> <adrl> <len of data> <data[0]> ... <data[len-1]>;
		msg->msg[1] = (uint8_t) ((drbcc->flashAddr >> 16) & 0xFF);
		msg->msg[2] = (uint8_t) ((drbcc->flashAddr >>  8) & 0xFF);
		msg->msg[3] = (uint8_t) ((drbcc->flashAddr >>  0) & 0xFF);
		msg->msg[4] = (uint8_t) (size);

		for (i = 0; i < size; i++)
		{
			msg->msg[5 + i] = (uint8_t) (drbcc->flashData[drbcc->flashPos + i]);
		}
		libdrbcc_add_msg_sec(drbcc, msg);
	}

	drbcc->flashAddr += size;
	drbcc->flashPos  += size;
	drbcc->flashLen  -= size;

	return DRBCC_RC_NOERROR;
}


DRBCC_RC_t libdrbcc_req_flash_write(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	unsigned int i, size;
	// target range word aligned?
	if (addr % 2 != 0 || len % 2 != 0)
	{
		return DRBCC_RC_UNSPEC_ERROR;
	}

	if (addr % CHUNK != 0) // 1st part to a 128 boundary
	{
		size = addr % CHUNK;
		size = CHUNK - size;
	}
	else
	{
		if (len >= CHUNK)
		{
			size = CHUNK;
		}
		else
		{
			size = len;
		}
	}

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 5 + size;
		msg->msg[0] = DRBCC_REQ_EXTFLASH_WRITE; // <mid> <adrh> <adrm> <adrl> <len of data> <data[0]> ... <data[len-1]>;
		msg->msg[1] = (uint8_t) ((addr >> 16) & 0xFF);
		msg->msg[2] = (uint8_t) ((addr >>  8) & 0xFF);
		msg->msg[3] = (uint8_t) ((addr >>  0) & 0xFF);
		msg->msg[4] = (uint8_t) (size);

		for (i = 0; i < size; i++)
		{
			msg->msg[5 + i] = (uint8_t) (*data++);
		}
		libdrbcc_add_msg_sec(drbcc, msg);
	}
	if (len <= size) // the one and only
	{
		return DRBCC_RC_NOERROR;
	}
	addr += size;
	len -= size;

	if (len)
	{
		drbcc->flashAddr = addr;
		drbcc->flashPos = 0;
		drbcc->flashLen = len;

		drbcc->flashData = malloc(len);
		memcpy(drbcc->flashData, data, len);
	}
	else
	{
		drbcc->flashAddr = 0;
		drbcc->flashPos = 0;
		drbcc->flashLen = 0;
	}

	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_req_flash_write(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned addr, unsigned len, uint8_t* data)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}
	drbcc->state = DRBCC_STATE_USER;

	if((0 != session) && drbcc->session_cb)
	{
		drbcc->session = drbcc_session++;
		*session = drbcc->session;
	}

	return libdrbcc_req_flash_write(drbcc, addr, len, data);
}

DRBCC_RC_t libdrbcc_req_flash_erase_block(DRBCC_t *drbcc, unsigned blocknum)
{
	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 3;
		msg->msg[0] = DRBCC_REQ_EXTFLASH_BLOCKERASE;
		msg->msg[1] = (uint8_t) ((blocknum >> 8) & 0xFF);
		msg->msg[2] = (uint8_t) ((blocknum >> 0) & 0xFF);

		libdrbcc_add_msg_sec(drbcc, msg);
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_req_flash_erase_block(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned blocknum)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}
	drbcc->state = DRBCC_STATE_USER;

	if((0 != session) && drbcc->session_cb)
	{
		drbcc->session = drbcc_session++;
		*session = drbcc->session;
	}

	return libdrbcc_req_flash_erase_block(drbcc, blocknum);
}

DRBCC_RC_t drbcc_invalidate_bctrl_fw(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_FW_INVALIDATE;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_restart_bctrl(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int when)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 2;
		msg->msg[0] = DRBCC_REQ_BCTRL_RESTART;
		msg->msg[1] = (uint8_t) when;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_request_bootloader_update(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = 1;
		msg->msg[0] = DRBCC_REQ_BL_UPDATE;

		libdrbcc_add_msg_prio(drbcc, msg);
		if((0 != session) && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
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
