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

#ifndef _DRBCC_UTILS_H_
#define _DRBCC_UTILS_H_

#include <drbcc.h> // definition of tracemask macros

DRBCC_RC_t libdrbcc_req_flash_read(DRBCC_t *drbcc, unsigned addr, unsigned len);

DRBCC_RC_t libdrbcc_flash_write(DRBCC_t *drbcc);

uint16_t libdrbcc_crc_ccitt_update(uint16_t crc, uint8_t data);

DRBCC_RC_t libdrbcc_req_flash_erase_block(DRBCC_t *drbcc, unsigned blocknum);

DRBCC_RC_t libdrbcc_req_flash_write(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data);

void libdrbcc_logpos_cb(DRBCC_t *drbcc);

void libdrbcc_readflash_cb(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data);

void libdrbcc_writeflash_cb(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t result);

void libdrbcc_add_msg_sec(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg);

void libdrbcc_add_msg_prio(DRBCC_t *drbcc, DRBCC_MESSAGE_t *msg);

#endif /* _DRBCC_UTILS_H_ */

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
