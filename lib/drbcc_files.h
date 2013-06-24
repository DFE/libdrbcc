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

#ifndef _DRBCC_FILES_
#define _DRBCC_FILES_

#include <stdint.h>
#include <drbcc_com.h>
#include <drbcc.h>

// Callbacks

DRBCC_RC_t drbcc_register_partition_cb(DRBCC_HANDLE_t h, DRBCC_PARTITION_TABLE_CB_t cb);

DRBCC_RC_t drbcc_register_progress_cb(DRBCC_HANDLE_t h, DRBCC_PROGRESS_CB_t cb);

DRBCC_RC_t drbcc_register_getlog_cb(DRBCC_HANDLE_t h, DRBCC_GETLOG_CB_t cb);

DRBCC_RC_t drbcc_register_getpos_cb(DRBCC_HANDLE_t h, DRBCC_GETPOS_CB_t cb);

DRBCC_RC_t drbcc_register_debug_get_cb(DRBCC_HANDLE_t h, DRBCC_DEBUG_GET_CB_t cb);

// Functions
DRBCC_RC_t drbcc_get_pos(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_clear_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

// get logs
DRBCC_RC_t drbcc_get_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int ring, int entries);

// put log entry
DRBCC_RC_t drbcc_put_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int ring, int len, uint8_t data[]);

// if magic is wrong a new one will be created
DRBCC_RC_t drbcc_get_partitiontable(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

// index: entry in partition table
DRBCC_RC_t drbcc_get_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index, const char filename[]);

// file type (bit 0-3 fileindex, bit 4-7 filetype)
DRBCC_RC_t drbcc_get_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type, const char filename[]);

// index: laufende 4bit Nummer bei Mehrfacheinträgen gleichen Typs
DRBCC_RC_t drbcc_put_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index, DRBCC_FLASHFILE_TYPES_t type, const char filename[]);

// file type (bit 0-3 fileindex, bit 4-7 filetype)
DRBCC_RC_t drbcc_put_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type, const char filename[]);

// index: entry in partition table
// 0: Ring-Log-Bereich, default-size 508 Blöcke (2032 KByte, 130048 Log-Einträge)
// 1: persistenter Log-Bereich, default-size 64 Blöcke (256 KByte, 16384 Log-Einträge)
// ...19: FW, BOOTLOADER, UBOOT, ...
DRBCC_RC_t drbcc_delete_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index);

// file type (bit 0-3 fileindex, bit 4-7 filetype)
DRBCC_RC_t drbcc_delete_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type);

DRBCC_RC_t drbcc_upload_firmware(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, const char *fname);

DRBCC_RC_t drbcc_upload_bootloader(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, const char *fname);

DRBCC_RC_t drbcc_eject_hd(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session);

DRBCC_RC_t drbcc_hd_power(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int onoff);

DRBCC_RC_t drbcc_gpi_power(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int onoff);

// debug
DRBCC_RC_t drbcc_debug_set(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int address, int len, uint8_t data[]);

DRBCC_RC_t drbcc_debug_get(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int address);

#endif /* _DRBCC_FILES_ */

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
