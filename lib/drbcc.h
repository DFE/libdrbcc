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

#ifndef _DRBCC_
#define _DRBCC_

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdint.h>
#include <sys/time.h>
#include <drbcc_com.h>

#ifdef HAVE_LIBDRTRACE
#include <drhiptrace.h>

#define DRBCC_TR_NONE       0x00000000UL
#define DRBCC_TR_ALL        0xFFFFFFFFUL
#define DRBCC_TR_TRANS      DRHIP_TCAT_2 // seriell transport
#define DRBCC_TR_PARAM      DRHIP_TCAT_3 // parameter parsing
#define DRBCC_TR_QUEUE      DRHIP_TCAT_4 // queue handling
#define DRBCC_TR_MSGS       DRHIP_TCAT_5 // msg handling
#else
#define DRBCC_TR_NONE       0x00000000
#define DRBCC_TR_ALL        0xFFFFFFFF
#define DRBCC_TR_TRANS      0x00000002 // seriell transport
#define DRBCC_TR_PARAM      0x00000003 // parameter parsing
#define DRBCC_TR_QUEUE      0x00000004 // queue handling
#define DRBCC_TR_MSGS       0x00000005 // msg handling

#endif

typedef void *DRBCC_HANDLE_t;

// Functions
DRBCC_RC_t drbcc_init(unsigned long tracemask);

DRBCC_RC_t drbcc_set_tracemask(unsigned long tracemask);

DRBCC_RC_t drbcc_init();

DRBCC_RC_t drbcc_term();

DRBCC_RC_t drbcc_open(DRBCC_HANDLE_t* h);

// register callbacks
DRBCC_RC_t drbcc_register_error_cb(DRBCC_HANDLE_t h, DRBCC_ERROR_CB_t cb);

DRBCC_RC_t drbcc_register_session_cb(DRBCC_HANDLE_t h, DRBCC_SESSION_CB_t cb);

DRBCC_RC_t drbcc_register_protocol_cb(DRBCC_HANDLE_t h, DRBCC_PROTOCOL_CB_t cb);

DRBCC_RC_t drbcc_register_id_cb(DRBCC_HANDLE_t h, DRBCC_ID_CB_t cb);

DRBCC_RC_t drbcc_session_activ(DRBCC_HANDLE_t h);
//
DRBCC_RC_t drbcc_sync(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_req_protocol(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_register_accel_event_cb(DRBCC_HANDLE_t h, DRBCC_ACCEL_EVENT_CB_t cb);

//Heartbeat, shutdown, etc
DRBCC_RC_t drbcc_heartbeat(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int timeout);

DRBCC_RC_t drbcc_shutdown(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int timeout);

DRBCC_RC_t drbcc_get_id_data(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

//GPIO
DRBCC_RC_t drbcc_register_status_cb(DRBCC_HANDLE_t h, DRBCC_STATUS_CB_t cb);

DRBCC_RC_t drbcc_set_gpo(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int gpo, unsigned int onoff);

DRBCC_RC_t drbcc_set_led(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int num, unsigned int color, unsigned int on_time, unsigned int off_time, unsigned int phase);

DRBCC_RC_t drbcc_get_status(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_register_hdOffRequest_cb(DRBCC_HANDLE_t h, DRBCC_STATUS_CB_t cb);

//clock
DRBCC_RC_t drbcc_register_rtc_cb(DRBCC_HANDLE_t h, DRBCC_RTC_CB_t cb);

DRBCC_RC_t drbcc_req_rtc(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_set_rtc(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, const struct tm* utctime);

// flash stuff
DRBCC_RC_t drbcc_register_flash_id_cb(DRBCC_HANDLE_t h, DRBCC_FLASH_ID_CB_t cb);

DRBCC_RC_t drbcc_req_flash_id(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_register_flash_read_cb(DRBCC_HANDLE_t h, DRBCC_READ_FLASH_CB_t cb);

DRBCC_RC_t drbcc_req_flash_read(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned addr, unsigned len);

DRBCC_RC_t drbcc_register_flash_write_cb(DRBCC_HANDLE_t h, DRBCC_WRITE_FLASH_CB_t cb);

DRBCC_RC_t drbcc_req_flash_write(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned addr, unsigned len, uint8_t* data);

DRBCC_RC_t drbcc_register_flash_erase_cb(DRBCC_HANDLE_t h, DRBCC_ERASE_FLASH_CB_t cb);

DRBCC_RC_t drbcc_req_flash_erase_block(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned blocknum);


// fw stuff
DRBCC_RC_t drbcc_invalidate_bctrl_fw(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_restart_bctrl(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int when);

DRBCC_RC_t drbcc_request_bootloader_update(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

// start stop etc.
DRBCC_RC_t drbcc_start(DRBCC_HANDLE_t h, void *context, const char *tty, DRBCC_BR_t br);

DRBCC_RC_t drbcc_stop(DRBCC_HANDLE_t h);

DRBCC_RC_t drbcc_close(DRBCC_HANDLE_t h);

DRBCC_RC_t drbcc_trigger(DRBCC_HANDLE_t h, int maxLoops);

// gets the error string
const char* drbcc_get_error_string(DRBCC_RC_t rc);

#endif /* _DRBCC_ */

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
