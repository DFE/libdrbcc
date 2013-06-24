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
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "drbcc_files.h"
#include "drbcc_ll.h"
#include "drbcc_com.h"
#include "drbcc_trace.h"
#include "drbcc_utils.h"

#ifdef HAVE_LIBDRTRACE
#else
#include "drbcc_trace.h"
#endif

#if USE_OPEN_BINARY
#define OX_BINARY O_BINARY
#else
#define OX_BINARY 0
#endif

extern int libdrbcc_initialized;

#define CHUNK 128

/* Speicher-Aufteilung: einfache 'Partitionstabelle' in Block 0, Page 0 (gesamt verwendet: 126 byte):

NEU: Ein Backup der 'Partitionstabelle' wird im Block 1, Page 0  abgelegt (Adresse 4096). Sie wird jeweils zuerst geschrieben und
bei Fehlern in der ersten PT zurückgelesen und die ertse wiederhergestellt.

* 6 byte Header:
	o 2 byte Magic zur Erkennung
	o 2 byte Version der Flashtabelle
	o 2 byte CRC (16bit DRBCC CRC, LSB first) über die folgende Tabelle:
* Tabelle mit 20 Einträgen mit je 6 Byte:
	o 1 byte Typ
		+ Bit7: 1=Länge wird in Anzahl der belegten Blöcke angegeben, 0=Länge wird in Byte angegeben
		+ Bit6..4 bei Block-Einträgen (Bit7==1):
			# 111 (7): Frei
			# 110 (6): Ring-Log-Bereich
			# 101 (5): persistenter Log-Bereich
			# 0..4: reserved for future use
		+ Bit6..4 bei Byte-Einträgen (Bit7==0):
			# 000 (0): Firmware-Update
			# 001 (1): Bootloader-Update
			# 010 (2): U-Boot-Image
			# 011 (3): U-Boot-Environment ?
			# 4..7: reserved for future use
		+ Bit3..0: laufende 4bit Nummer bei Mehrfacheinträgen gleichen Typs (leer: 0xf)
	o 2 byte Start-Block; LSB first, 4K-Block-weise, ab Block 4
	o 3 byte Länge: LSB first, bei Block-Einträgen: Anzahl der 4K-Blöcke, bei Byte-Einträgen: Länge in Byte; Anzahl der belegten Blöcke ist dann ((Länge + 0xfff)/0x1000)

* Ring-Log-Bereich ist immer der erste Eintrag, fest in BCTRL-Firmware: Block 4 bis 511, 508 Blöcke (2032 KByte, 130048 Log-Einträge)

* unbenutzt: persistenter Log-Bereich ist immer der zweite Eintrag, default-size 64 Blöcke (256 KByte, 16384 Log-Einträge)

Block 1,2,3: reserved for future use Block 4..1023: Datenbereich

* Platzhalter-Einträge: 4..511: Ring-Log-Bereich, unbenutzt: 512..575: persistenter Log-Bereich
* dann noch verfügbar: 448 Blöcke
* Platzbedarf Firmware-Update: 32 Blöcke (128KByte), Bootloader-Update: 2 Blöcke (8KByte), U-Boot-Image: 64 Blöcke (256KByte)
*/


typedef struct
{
	int start;
	int end;
	int free;
} FREE_USED_TABLE_t;

DRBCC_SESSION_t drbcc_session = 1;

#define EMPTY_ENTRY 0xF

#define isEmpty(x) ((x.type.typeinfo >> 4) == EMPTY_ENTRY)

void libdrbcc_dump(uint8_t data[], unsigned int len)
{
	// hexdump to stdout
	unsigned i = 0;
	for (i = 0; i < len; i++)
	{
		if ((i % 16) == 0)
		{
			fprintf(stdout, "0x%04X:", i );
		}
		fprintf(stdout, " %02X", data[i]);
		if ((i % 16) == 15)
		{
			fprintf(stdout, "\n");
		}
	}
	if (i % 16)
	{
		fprintf(stdout, "\n");
	}
}

void libdrbcc_create_partition(DRBCC_t *drbcc)
{
	int i = 0;
	int j = 0;
	uint8_t data[128];
	DRBCC_PARTENTRY_t e[20];
	uint16_t crc = 0xffff;

	memset(e, 0xFF, sizeof(e));

	data[i++] = DRBCC_PART_MAGIC1;	// magic
	data[i++] = DRBCC_PART_MAGIC2;
	data[i++] = 1;		// version
	data[i++] = 0;
	i++;				// crc
	i++;

	// dieser Eintrag ist ein reiner Platzhalter, die BCTRL-Firmware ist auf diesen Bereich 'festverdrahtet' (MRE 11.4.2012)
	// Werte NICHT ändern, der BCTRL schreibt die Logdaten IMMER in diesen Bereich!!!!
	e[0].type.bits.blocktype = 1;
	e[0].type.bits.type = DRBCC_FLASHBLOCK_T_RING_LOG;
	e[0].type.bits.idx = 0;
	e[0].startblock = 4;
	e[0].length = 508;

	// dieser Bereich ist ungenutzt
	e[1].type.bits.blocktype = 1;
	e[1].type.bits.type = DRBCC_FLASHBLOCK_T_PERS_LOG;
	e[1].type.bits.idx = 0;
	e[1].startblock = 512;
	e[1].length = 64;

	// build data
	for (j = 0; j < 20; j++)
	{
		data[i++] = e[j].type.typeinfo;
		data[i++] = (uint8_t) ((e[j].startblock >> 0) & 0xFF);
		data[i++] = (uint8_t) ((e[j].startblock >> 8) & 0xFF);
		data[i++] = (uint8_t) ((e[j].length >>  0) & 0xFF);
		data[i++] = (uint8_t) ((e[j].length >>  8) & 0xFF);
		data[i++] = (uint8_t) ((e[j].length >> 16) & 0xFF);
	}
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	data[4] = (uint8_t) (crc & 0xFF);
	data[5] = (uint8_t) ((crc >> 8) & 0xFF);

	libdrbcc_req_flash_erase_block(drbcc, 1);
	libdrbcc_req_flash_write(drbcc, 4096, 128, data);
	libdrbcc_req_flash_erase_block(drbcc, 0);
	libdrbcc_req_flash_write(drbcc, 0, 128, data);
}

DRBCC_RC_t libdrbcc_request_partition(DRBCC_t *drbcc)
{
	// request 1st 128 Bytes of flash
	return libdrbcc_req_flash_read(drbcc, 0, 128);
}

DRBCC_RC_t libdrbcc_request_partition2(DRBCC_t *drbcc)
{
	// request 1st 128 Bytes of flash
	return libdrbcc_req_flash_read(drbcc, 4096, 128);
}

void libdrbcc_got_partition(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	int i;
	uint16_t crc = 0xffff;

	// check addr
	if (addr != 0) //
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Wrong flash space used for partition table!");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}
	if (len < 126)
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Partition table is to short!");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check magic
	if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
	{
		libdrbcc_create_partition(drbcc);
		libdrbcc_request_partition(drbcc);

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table, creating new one");
		}
		return;
	}

	// check crc
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	if (((crc & 0xff) != data[4]) || (((crc >> 8) & 0xff) != data[5]))
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "CRC error in flash partition table");
		}
		// HACK? return;
	}

	// ignore version info
	data += 6;

	// build entries
	DRBCC_PARTENTRY_t e[20];
	for (i = 0; i < 20; i++)
	{
		e[i].type.typeinfo	= data[i*6];
		e[i].startblock		= data[i*6 + 1] | (data[i*6 + 2] << 8);
		e[i].length		= data[i*6 + 3] | (data[i*6 + 4] << 8) | (data[i*6 + 5] << 16); // size in blocks or bytes
	}

	if (drbcc->partitiontable_cb)
	{
		drbcc->state = DRBCC_STATE_USER;
		drbcc->partitiontable_cb(drbcc->context, 20, e);
	}
	if (drbcc->session_cb)
	{
		drbcc->session_cb(drbcc->context, drbcc->session, 1);
	}
	drbcc->session = 0;
}

void libdrbcc_delete(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	int i;
	uint16_t crc = 0xffff;

	// check addr
	if (addr != 0) //
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Wrong flash space used for partition table!");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}
	if (len < 126)
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Delete: partition table is to short!");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check magic
	if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
	{
		// TODO create partition table?

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check crc
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	if (((crc & 0xff) != data[4]) || (((crc >> 8) & 0xff) != data[5]))
	{
		// TODO repair partition table?

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "CRC error in flash partition table");
		}
	}
	// ignore version info

	// delete entry
	if((drbcc->curFileIndex & 0x80000000) == 0)
	{
		// delete file by partition table index
		data[6 + (drbcc->curFileIndex * 6)] = 0xFF;
		data[7 + (drbcc->curFileIndex * 6)] = 0xFF;
		data[8 + (drbcc->curFileIndex * 6)] = 0xFF;
		data[9 + (drbcc->curFileIndex * 6)] = 0xFF;
		data[10 + (drbcc->curFileIndex * 6)] = 0xFF;
		data[11 + (drbcc->curFileIndex * 6)] = 0xFF;
	}
	else
	{
		// delete file by file type
		for (i = 0; i < 20; i++)
		{
			DRBCC_PARTENTRY_t e_curr;
			e_curr.type.typeinfo = data[6 + i*6];
			e_curr.startblock    = data[6 + i*6+1] | (data[6 + i*6+2] << 8);
			e_curr.length        = data[6 + i*6+3] | (data[6 + i*6+4] << 8) | (data[6 + i*6+5] << 16); // size in blocks or bytes

			// reuse entry of same type
			if ((e_curr.type.bits.type == (drbcc->curFileType & 0x07)) &&
				(e_curr.type.bits.blocktype == ((drbcc->curFileType>>3) & 0x01)) &&
				(e_curr.type.bits.idx == (drbcc->curFileIndex & 0xF)))
			{
				data[6 + i * 6 + 0] = 0xFF;
				data[6 + i * 6 + 1] = 0xFF;
				data[6 + i * 6 + 2] = 0xFF;
				data[6 + i * 6 + 3] = 0xFF;
				data[6 + i * 6 + 4] = 0xFF;
				data[6 + i * 6 + 5] = 0xFF;
				break;
			}
		}
	}

	crc = 0xffff;
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	data[4] = (uint8_t) (crc & 0xFF);
	data[5] = (uint8_t) ((crc >> 8) & 0xFF);

	libdrbcc_req_flash_erase_block(drbcc, 1);
	libdrbcc_req_flash_write(drbcc, 4096, 128, data);
	libdrbcc_req_flash_erase_block(drbcc, 0);
	libdrbcc_req_flash_write(drbcc, 0, 128, data);
}

void libdrbcc_get_file(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	int i;
	uint16_t crc = 0xffff;

	// check addr
	if (addr != 0) // requested data
	{
		int fd = open(drbcc->curFilename, O_RDWR | O_CREAT | OX_BINARY, 0666 );
		if (fd != -1)
		{
			ssize_t wr;
			if (-1 == lseek(fd, addr - drbcc->curFilestart, SEEK_SET))
			{
				if (drbcc->error_cb)
				{
					drbcc->error_cb(drbcc->context, "lseek to offset in file failed");
				}
				if (drbcc->session_cb)
				{
					drbcc->session_cb(drbcc->context, drbcc->session, 1);
				}
				drbcc->session = 0;
				TRACE_WARN("lseek to offset %u in file %s failed", addr, drbcc->curFilename);
			}
			else if ((ssize_t)len == (wr = write(fd, data, len)))
			{
				drbcc->curFilelength += len;

				if (drbcc->progress_cb)
				{
					drbcc->progress_cb(drbcc->context, drbcc->curFilelength, drbcc->maxFilelength);
				}
				if (drbcc->curFilelength == drbcc->maxFilelength)
				{
					drbcc->state = DRBCC_STATE_USER;
					if (drbcc->error_cb)
					{
						drbcc->error_cb(drbcc->context, "Get flash file successfully done");
					}
					if (drbcc->session_cb)
					{
						drbcc->session_cb(drbcc->context, drbcc->session, 1);
					}
					drbcc->session = 0;
				}
				else
				{
					libdrbcc_req_flash_read(drbcc, drbcc->flashAddr, drbcc->flashLen);
				}
			}
			else
			{
				TRACE_WARN("writing flash data to file %s failed", drbcc->curFilename);
				if (drbcc->error_cb)
				{
					char s[512];
					memset(s, 0, sizeof(s));
					snprintf(s, sizeof(s)-1, "writing %d byte(s) flash data to file %s failed. write returned %zd", len, drbcc->curFilename, wr);
					drbcc->error_cb(drbcc->context, s);
				}
				if (drbcc->session_cb)
				{
					drbcc->session_cb(drbcc->context, drbcc->session, 1);
				}
				drbcc->session = 0;
			}
			close(fd);
			return;
		}
		else
		{
			TRACE_WARN("open file %s failed", drbcc->curFilename);
			if (drbcc->error_cb)
			{
				drbcc->error_cb(drbcc->context, "open file failed");
			}
			if (drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
			drbcc->session = 0;
			return;
		}
	}

	// check magic
	if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
	{
		// TODO create partition table?

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check crc
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	if (((crc & 0xff) != data[4]) || (((crc >> 8) & 0xff) != data[5]))
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "CRC error in flash partition table");
		}
	}

	DRBCC_PARTENTRY_t e;
	memset(&e, 0xFF, sizeof(e));


	if((drbcc->curFileIndex & 0x80000000) == 0)
	{
		// get file by partition table index
		data += (6 + drbcc->curFileIndex * 6);

		e.type.typeinfo	= data[0];
		e.startblock	= data[1] | (data[2] << 8);
		e.length	= data[3] | (data[4] << 8) | (data[5] << 16); // size in blocks or bytes

		drbcc->curFilelength = 0;
		drbcc->maxFilelength = e.length;
	}
	else
	{
		// get file by file type
		data += 6;
		for (i = 0; i < 20; i++)
		{
			DRBCC_PARTENTRY_t e_curr;
			e_curr.type.typeinfo = data[i*6];
			e_curr.startblock    = data[i*6+1] | (data[i*6+2] << 8);
			e_curr.length        = data[i*6+3] | (data[i*6+4] << 8) | (data[i*6+5] << 16); // size in blocks or bytes

			// reuse entry of same type
			if ((e_curr.type.bits.type == (drbcc->curFileType & 0x07)) &&
				(e_curr.type.bits.blocktype == ((drbcc->curFileType>>3) & 0x01)) &&
				(e_curr.type.bits.idx == (drbcc->curFileIndex & 0xF)))
			{
				e.type.typeinfo      = e_curr.type.typeinfo;
				e.startblock         = e_curr.startblock;
				e.length             = e_curr.length;
				drbcc->curFilelength = 0;
				drbcc->maxFilelength = e.length;
				break;
			}
		}
	}

	if ((!isEmpty(e)) && (e.length > 0))
	{
		drbcc->curFilestart = e.startblock * 0x1000;

		if (e.type.bits.blocktype)
		{
			drbcc->maxFilelength = e.length * 0x1000;

			libdrbcc_req_flash_read(drbcc, drbcc->curFilestart, drbcc->maxFilelength);
		}
		else
		{
			drbcc->maxFilelength = e.length;

			libdrbcc_req_flash_read(drbcc, drbcc->curFilestart, drbcc->maxFilelength);
		}
	}
	else
	{
		if (drbcc->error_cb)
		{
			if (isEmpty(e))
			{
				drbcc->error_cb(drbcc->context, "Invalid flash file, empty entry");
			}
			else
			{
				drbcc->error_cb(drbcc->context, "Invalid flash file, size 0");
			}
			if (drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
		}
		drbcc->session = 0;
		drbcc->state = DRBCC_STATE_USER;
	}
}

void libdrbcc_put_file(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	unsigned int i, j;
	int k;
	uint16_t crc = 0xffff;

	// check addr
	if (addr != 0)
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Wrong flash space used for partition table!");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check magic
	if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
	{
		// TODO create partition table?

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check crc
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	if (((crc & 0xff) != data[4]) || (((crc >> 8) & 0xff) != data[5]))
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "CRC error in flash partition table");
		}
	}

	int emptyEntry = -1;
	uint8_t blocks[1024];
	int endblock;
	DRBCC_PARTENTRY_t e[20];

	memset(blocks, 0, sizeof(blocks));
	blocks[0] = 1; // used for partition table
	blocks[1] = 1; // reserved
	blocks[2] = 1; // reserved
	blocks[3] = 1; // reserved

	// Build partition table and find empty entry
	data += 6;
	for (i = 0; i < 20; i++)
	{
		e[i].type.typeinfo	= data[i*6];
		e[i].startblock		= data[i*6+1] | (data[i*6+2] << 8);
		e[i].length			= data[i*6+3] | (data[i*6+4] << 8) | (data[i*6+5] << 16); // size in blocks or bytes

		if ((emptyEntry == -1) && (isEmpty(e[i])))
		{
			emptyEntry = i;
		}

		// reuse entry of same type
		if ((e[i].type.bits.type == (drbcc->curFileType & 0x07)) &&
			(e[i].type.bits.blocktype == ((drbcc->curFileType>>3) & 0x01)) &&
			(e[i].type.bits.idx  == drbcc->curFileIndex))
		{
			emptyEntry = i;
			e[i].type.typeinfo = 0xFF;
		}

		if (!isEmpty(e[i]))
		{
			if (e[i].type.bits.blocktype)
			{
				endblock = e[i].startblock + e[i].length;
			}
			else
			{
				endblock = e[i].startblock + ((e[i].length + 0xfff) / 0x1000);
			}
			// mark used blocks
			for (k = e[i].startblock; k < endblock; k++)
			{
				blocks[k] = 1;
			}
		}
	}

	//libdrbcc_dump(blocks, sizeof(blocks));

	FREE_USED_TABLE_t free_used[40];
	int last = 1; // we start with used blocks (partition table & reserved)
	j = 0;

	if (emptyEntry == -1)
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Put flash file failed, partition table full");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		drbcc->state = DRBCC_STATE_USER;
		return;
	}
	else
	{
		// collect free and used in blocks table
		for (i = 4; i < 1024; i++)
		{
			if (blocks[i] && last)
			{
				free_used[j].end = i;
			}
			else if (blocks[i])
			{
				j++;
				last = 1;
				free_used[j].start = i;
				free_used[j].end = i;
				free_used[j].free = 0;
			}
			else if (last)
			{
				j++;
				last = 0;
				free_used[j].start = i;
				free_used[j].end = i;
				free_used[j].free = 1;
			}
			else
			{
				free_used[j].end = i;
			}
		}

		int requiredBlocks = (drbcc->maxFilelength + 0xfff) / 0x1000;
		int foundsize = 0, found = 0;

		// find best fitting area in used/free table
		for (i = 0; i <= j; i++)
		{
			int blocksize = (free_used[i].end - free_used[i].start) + 1;
			//fprintf(stderr, "start 0x%06x, end 0x%06x, size %i, free: %u\n", free_used[i].start, free_used[i].end, blocksize, free_used[i].free);

			if (free_used[i].free && (blocksize >= requiredBlocks))
			{
				if (found)
				{
					if (blocksize < foundsize)
					{
						// this one is better
						found = i;
						foundsize = blocksize;
					}
				}
				else
				{
					found = i;
					foundsize = blocksize;
				}
			}
		}


		/*fprintf(stderr, "found %i for %i blocks start 0x%06x, end 0x%06x, free: %u using pos %i in partition table\n", found, requiredBlocks,
			free_used[found].start, free_used[found].end, free_used[found].free, emptyEntry);*/

		if (found > 0)
		{
			uint8_t buf[CHUNK];
			int fd = open(drbcc->curFilename, O_RDONLY | OX_BINARY);

			unsigned int write_addr = free_used[found].start * 0x1000;

			if (fd != -1)
			{
				for (i = 0; i < drbcc->maxFilelength; i += CHUNK)
				{
					if (i % 0x1000 == 0) // new block
					{
						//fprintf(stderr, "erase data block %i\n", (i / 0x1000) + free_used[found].start);
						libdrbcc_req_flash_erase_block(drbcc, (i / 0x1000) + free_used[found].start);
					}

					DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

					if (msg != NULL)
					{
						ssize_t rd;
						if (write_addr + CHUNK <= (free_used[found].start * 0x1000) + drbcc->maxFilelength)
						{
							len = (uint8_t) CHUNK;
						}
						else
						{
							len = (uint8_t) (drbcc->maxFilelength % CHUNK);
						}
						msg->msg_len = 5 + len;
						msg->msg[0] = DRBCC_REQ_EXTFLASH_WRITE;
						msg->msg[1] = (uint8_t) ((write_addr >> 16) & 0xFF);
						msg->msg[2] = (uint8_t) ((write_addr >>  8) & 0xFF);
						msg->msg[3] = (uint8_t) ((write_addr >>  0) & 0xFF);
						msg->msg[4] = (uint8_t) len;

						//fprintf(stderr, " sending %x , %i\n", write_addr, msg->msg[4]);
						write_addr = write_addr + len;

						rd = read(fd, buf, len);
						if (rd == (ssize_t)len)
						{
							for (j = 0; j < len; j++)
							{
								msg->msg[5 + j] = buf[j];
							}
						}
						else
						{
							if (drbcc->error_cb)
							{
								char s[512];
								memset(s, 0, sizeof(s));
								sprintf(s, "Cant read %d bytes from file during put flash file operation. read returned %zd", len, rd);
								drbcc->error_cb(drbcc->context, s);
							}
							if (drbcc->session_cb)
							{
								drbcc->session_cb(drbcc->context, drbcc->session, 1);
							}
							drbcc->state = DRBCC_STATE_USER;
							close(fd);
							return;
						}

						libdrbcc_add_msg_sec(drbcc, msg);
					}
					else
					{
						if (drbcc->error_cb)
						{
							drbcc->error_cb(drbcc->context, "Out of memory during get file operation");
						}
						if (drbcc->session_cb)
						{
							drbcc->session_cb(drbcc->context, drbcc->session, 1);
						}
						drbcc->state = DRBCC_STATE_USER;
						close(fd);
						return;
					}
				}
				close(fd);

				TRACE(DRBCC_TR_TRANS, "change partition table entry %i start 0x%06x length %i", emptyEntry, free_used[found].start, drbcc->maxFilelength);

				e[emptyEntry].type.bits.blocktype	= 0;
				e[emptyEntry].type.bits.type		= drbcc->curFileType;
				e[emptyEntry].type.bits.idx			= drbcc->curFileIndex;
				e[emptyEntry].startblock			= free_used[found].start;
				e[emptyEntry].length				= drbcc->maxFilelength;  // size in bytes

				uint8_t data[128];
				uint16_t crc = 0xffff;

				i = 0;
				data[i++] = DRBCC_PART_MAGIC1;	// magic
				data[i++] = DRBCC_PART_MAGIC2;
				data[i++] = 1;		// version
				data[i++] = 0;
				i++;				// crc
				i++;

				// build free_used
				for (j = 0; j < 20; j++)
				{
					data[i++] = e[j].type.typeinfo;
					data[i++] = (uint8_t) ((e[j].startblock >> 0) & 0xFF);
					data[i++] = (uint8_t) ((e[j].startblock >> 8) & 0xFF);
					data[i++] = (uint8_t) ((e[j].length >>  0) & 0xFF);
					data[i++] = (uint8_t) ((e[j].length >>  8) & 0xFF);
					data[i++] = (uint8_t) ((e[j].length >> 16) & 0xFF);
				}
				for (i = 6; i < 126; i++)
				{
					crc = libdrbcc_crc_ccitt_update(crc, data[i]);
				}
				data[4] = (uint8_t) (crc & 0xFF);
				data[5] = (uint8_t) ((crc >> 8) & 0xFF);

				//libdrbcc_dump(data, sizeof(data));
				libdrbcc_req_flash_erase_block(drbcc, 1);
				libdrbcc_req_flash_write(drbcc, 4096, 128, data);
				libdrbcc_req_flash_erase_block(drbcc, 0);
				libdrbcc_req_flash_write(drbcc, 0, 128, data);
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				drbcc->error_cb(drbcc->context, "Put flash file failed, no space left");
			}
			if (drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
			drbcc->session = 0;
			drbcc->state = DRBCC_STATE_USER;
		}
	}
}

static void handle_logentry(DRBCC_t *drbcc, unsigned int pos, int len, uint8_t* buf)
{
	if (0xFF == drbcc->logwrapflag)
	{
		if (pos < drbcc->start) { return; }
	}
	else
	{
		unsigned int curr_log_pos = drbcc->curFileIndex * 0x1000 / 16 + drbcc->logentry;
		if (curr_log_pos > drbcc->start)
		{
			if(pos < drbcc->start) { return; }
		}
		else
		{
			if((curr_log_pos < pos) && (pos < drbcc->start)) { return; }
		}
	}
	if (drbcc->getlog_cb)
	{
		drbcc->getlog_cb(drbcc->context, pos, len, &buf[0]);
	}
}

static unsigned int calculate_start_addr_wrap(DRBCC_t *drbcc, unsigned int from_x, unsigned curr_log_pos)
{
	unsigned start_addr;

	if(from_x >= curr_log_pos)
	{
		unsigned int next_entry = drbcc->curFileIndex + 1;
		next_entry %= (drbcc->maxFilelength / 0x1000); // take ring memory log into account
		next_entry *= 0x1000; // block to addr offset
		next_entry /= 16; // addr offset to entry index

		if(0 == next_entry) // was last block, ring starts now at offset 0
		{
			drbcc->start = 0;
			start_addr = 0;
		}
		else if(next_entry < from_x) // request entry is in upper ring buffer
		{
			drbcc->start = from_x;
			start_addr = ((drbcc->start*16) & (~0x7F)); // alignment 128
		}
		else // request entry is invalid, start with next valid entry
		{
			drbcc->start = next_entry;
			start_addr = (drbcc->start*16);
		}
	}
	else
	{
		drbcc->start = from_x;
		start_addr = ((drbcc->start*16) & (~0x7F)); // alignment 128
	}
	return start_addr;
}

void libdrbcc_get_log(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	unsigned int i;
	uint16_t crc = 0xffff;

	// check addr
	if (addr != 0)
	{
		unsigned pos = (addr - drbcc->curFilestart) / 16;
		for (i = 0; i+16 <= len; i += 16)
		{
			if (data[i] != 0xff)
			{
				if (data[i] != DRBCC_E_EXTENSION)
				{
					// if (data[i + 1] == 0xff) this is a invalid entry made by AVR programmer
					if ((data[i + 1] != 0xff) && (data[i + 8] > sizeof(drbcc->dlpf.data)))
					{
						if (drbcc->logdata)
						{
							free(drbcc->logdata);
						}
						drbcc->logdata = malloc(data[i + 8] * 3);
						memcpy(drbcc->logdata, &data[i], 16);
						drbcc->logrest = data[i + 8] - sizeof(drbcc->dlpf.data);
						drbcc->logindex = 16;
						drbcc->logpos = (pos + i/16);
					}
					else
					{
						handle_logentry(drbcc, pos + i/16, data[i+8]+9, &data[i]);
					}
				}
				else
				{
					if(NULL == drbcc->logdata)
					{
						// unexpected extention log
						handle_logentry(drbcc, pos + i/16, 16, &data[i]);
					}
					// extension log seq ...
					else if (drbcc->logrest > 15)
					{
						memcpy(drbcc->logdata + drbcc->logindex, &data[i + 1], 15);
						drbcc->logindex += 15;
						drbcc->logrest -= 15;
					}
					else
					{
						memcpy(drbcc->logdata + drbcc->logindex, &data[i + 1], drbcc->logrest);
						// complete
						handle_logentry(drbcc, drbcc->logpos, drbcc->logdata[8]+9, drbcc->logdata);
						free(drbcc->logdata);
						drbcc->logdata = NULL;
					}
				}
			}
			else  // empty log entry
			{
				// stop getting more data
				if ((addr + i) == (drbcc->curFilestart + drbcc->curFileIndex * 0x1000 + drbcc->logentry * 16))
				{
					drbcc->state = DRBCC_STATE_USER;
					if (drbcc->session_cb)
					{
						drbcc->session_cb(drbcc->context, drbcc->session, 1);
					}
					drbcc->session = 0;
					return;
				}
			}
		}


		if ((addr + 128) == (drbcc->curFilestart + drbcc->curFileIndex * 0x1000 + drbcc->logentry * 16))
		{
			drbcc->state = DRBCC_STATE_USER;
			if (drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
			drbcc->session = 0;
			return;
		}

		if ((addr + 128) == (drbcc->curFilestart + drbcc->maxFilelength)) // end of ring
		{
			libdrbcc_req_flash_read(drbcc, drbcc->curFilestart, 128);	// now get entries from start of ring
		}
		else
		{
			libdrbcc_req_flash_read(drbcc, addr + 128, 128);
		}

		return;
	}

	// check magic
	if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
	{
		// TODO create partition table?

		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table");
		}
		drbcc->state = DRBCC_STATE_USER;
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		return;
	}

	// check crc
	for (i = 6; i < 126; i++)
	{
		crc = libdrbcc_crc_ccitt_update(crc, data[i]);
	}
	if (((crc & 0xff) != data[4]) || (((crc >> 8) & 0xff) != data[5]))
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "CRC error in flash partition table");
		}
	}

	int logEntry = -1;
	DRBCC_PARTENTRY_t e[20];

	// Build partition table and find log entry
	data += 6;
	for (i = 0; i < 20; i++)
	{
		e[i].type.typeinfo	= data[i*6];
		e[i].startblock		= data[i*6+1] | (data[i*6+2] << 8);
		e[i].length			= data[i*6+3] | (data[i*6+4] << 8) | (data[i*6+5] << 16); // size in blocks or bytes

		if (drbcc->logtype &&
			(e[i].type.bits.type == DRBCC_FLASHBLOCK_T_RING_LOG) &&
			(e[i].type.bits.blocktype ==  0x01) )
		{
			logEntry = i;
			break;
		}
		if (!drbcc->logtype &&
			(e[i].type.bits.type == DRBCC_FLASHBLOCK_T_PERS_LOG) &&
			(e[i].type.bits.blocktype ==  0x01) )
		{
			logEntry = i;
			break;
		}
	}

	//libdrbcc_dump(blocks, sizeof(blocks));

	if (logEntry == -1)
	{
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "Get log failed, partition entry missing");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
		drbcc->state = DRBCC_STATE_USER;
		return;
	}
	else
	{
		int entries = drbcc->entries;
		unsigned int abs_entries = entries >= 0 ? entries : -entries;
		unsigned int curr_log_pos;
		unsigned int start_addr;
		drbcc->curFilestart = e[logEntry].startblock * 0x1000;
		drbcc->maxFilelength =  e[logEntry].length * 0x1000;
		// cur pos in drbcc->curFileIndex as abs block number
		drbcc->curFileIndex  = drbcc->curFileIndex - (drbcc->curFilestart / 0x1000);
		curr_log_pos = drbcc->curFileIndex * 0x1000 / 16 + drbcc->logentry;

		if (0xFF == drbcc->logwrapflag)
		{  // no wrap, start from block 0;
			if(abs_entries >= (drbcc->maxFilelength/16)) // all
			{
				start_addr = 0;
			}
			else if(0 > entries) // last x entries
			{
				unsigned int last_x = -entries;
				if(last_x >= curr_log_pos)
				{
					drbcc->start = 0;
					start_addr = 0;
				}
				else
				{
					drbcc->start = (curr_log_pos-last_x);
					start_addr =  ((drbcc->start*16) & (~0x7F)); // alignment 128
				}
			}
			else // from entry x to now
			{
				unsigned int from_x = entries;
				if(from_x >= curr_log_pos)
				{
					drbcc->start = 0;
					start_addr = 0;
				}
				else
				{
					drbcc->start = from_x;
					start_addr = ((drbcc->start*16) & (~0x7F)); // alignment 128
				}
			}
		}
		else
		{
			if(abs_entries >= (drbcc->maxFilelength/16)) // all
			{
				start_addr = drbcc->curFileIndex;
				start_addr += 1; // log starts from next block
				start_addr %= (drbcc->maxFilelength / 0x1000); // take ring memory log into account
				start_addr *= 0x1000; // block to addr offset
				drbcc->start = (start_addr/16);
			}
			else if(0 > entries) // last x entries
			{
				unsigned int last_x = -entries;
				if(last_x > curr_log_pos)
				{
					unsigned int rest = (last_x - curr_log_pos);
					unsigned int from_x = (drbcc->maxFilelength / 16) - 1 - rest; // -1, because pos is an index
					start_addr = calculate_start_addr_wrap(drbcc, from_x, curr_log_pos);
				}
				else
				{
					start_addr = calculate_start_addr_wrap(drbcc, curr_log_pos-last_x, curr_log_pos);
				}
			}
			else // from entry x to now
			{
				start_addr = calculate_start_addr_wrap(drbcc, entries, curr_log_pos);
			}
		}

		libdrbcc_req_flash_read(drbcc, drbcc->curFilestart + start_addr, 128);
	}
}

void libdrbcc_readflash_cb(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t* data)
{
	static int retry = 0;

	// check magic
	if (retry == 0 && addr == 0 && ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1]))) // Check partition table magic
	{
		libdrbcc_request_partition2(drbcc);
		retry = 1;
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No magic in flash partition table, try other one");
		}
		return;
	}

	if (addr == 4096)
	{
		if ((DRBCC_PART_MAGIC1 != data[0]) || (DRBCC_PART_MAGIC2 != data[1])) // Check partition table magic
		{
			if (drbcc->error_cb)
			{
				drbcc->error_cb(drbcc->context, "No magic in 2nd flash partition table, try without");
				addr = 0;
			}
		}
		else
		{
			libdrbcc_req_flash_erase_block(drbcc, 0);
			libdrbcc_req_flash_write(drbcc, 0, 128, data);
			libdrbcc_req_flash_read(drbcc, 0, 128);
			return;
		}
	}
	retry = 0;

	switch (drbcc->state)
	{
	case DRBCC_STATE_PARTITION_REQ:
		libdrbcc_got_partition(drbcc, addr, len, data);
		break;
	case DRBCC_STATE_DELETE_FILE:
		libdrbcc_delete(drbcc, addr, len, data);
		break;
	case DRBCC_STATE_GET_FILE:
		libdrbcc_get_file(drbcc, addr, len, data);
		break;
	case DRBCC_STATE_PUT_FILE:
		libdrbcc_put_file(drbcc, addr, len, data);
		break;
	case DRBCC_STATE_GET_LOG:
		libdrbcc_get_log(drbcc, addr, len, data);
		break;
	default:
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No handler for read flash callback");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
	}
}

void libdrbcc_logpos_cb(DRBCC_t *drbcc)
{
	switch(drbcc->state)
	{
	case DRBCC_STATE_GET_LOG:
		libdrbcc_request_partition(drbcc);
		break;
	default:
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No handler for logpos callback");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
	}
}
void libdrbcc_writeflash_cb(DRBCC_t *drbcc, unsigned addr, unsigned len, uint8_t result)
{
	switch(drbcc->state)
	{
	case DRBCC_STATE_PARTITION_REQ:
		// new partition table created, nothing to do
		break;
	case DRBCC_STATE_DELETE_FILE:
		drbcc->state = DRBCC_STATE_USER;
		if (drbcc->error_cb)
		{
			if (result)
			{
				drbcc->error_cb(drbcc->context, "Flash file successfully deleted.");
			}
			else
			{
				drbcc->error_cb(drbcc->context, "Delete flash file failed!");
			}
		}
		if (drbcc->session_cb)
		{
			if (result)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
			else
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 0);
			}
		}
		drbcc->session = 0;
		break;
	case DRBCC_STATE_PUT_FILE:
		if (result)
		{
			if (addr != 0 && addr != 4096)
			{
				drbcc->curFilelength += len;
			}
			if (drbcc->progress_cb)
			{
				drbcc->progress_cb(drbcc->context, drbcc->curFilelength, drbcc->maxFilelength);
			}
			if (addr == 0 && drbcc->curFilelength == drbcc->maxFilelength)
			{
				if (drbcc->error_cb)
				{
					drbcc->error_cb(drbcc->context, "Put flash file successfully done");
				}
				if (drbcc->session_cb)
				{
					drbcc->session_cb(drbcc->context, drbcc->session, 1);
				}
				drbcc->session = 0;
			}
		}
		else
		{
			if (drbcc->error_cb)
			{
				drbcc->error_cb(drbcc->context, "Flash file error result");
			}
			if (drbcc->session_cb)
			{
				drbcc->session_cb(drbcc->context, drbcc->session, 1);
			}
			drbcc->session = 0;
		}
		break;
	default:
		if (drbcc->error_cb)
		{
			drbcc->error_cb(drbcc->context, "No handler for write flash callback");
		}
		if (drbcc->session_cb)
		{
			drbcc->session_cb(drbcc->context, drbcc->session, 1);
		}
		drbcc->session = 0;
	}
}

DRBCC_RC_t drbcc_register_partition_cb(DRBCC_HANDLE_t h, DRBCC_PARTITION_TABLE_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->partitiontable_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_progress_cb(DRBCC_HANDLE_t h, DRBCC_PROGRESS_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->progress_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_getlog_cb(DRBCC_HANDLE_t h, DRBCC_GETLOG_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->getlog_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_getpos_cb(DRBCC_HANDLE_t h, DRBCC_GETPOS_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->getpos_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_register_debug_get_cb(DRBCC_HANDLE_t h, DRBCC_DEBUG_GET_CB_t cb)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->debug_get_cb = cb;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_get_partitiontable(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (drbcc->read_flash_cb || drbcc->erase_flash_cb || drbcc->write_flash_cb)
	{
		return DRBCC_RC_CBREGISTERED;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->session = drbcc_session++;
	*session = drbcc->session;
	drbcc->state = DRBCC_STATE_PARTITION_REQ;

	return libdrbcc_request_partition(drbcc);
}

DRBCC_RC_t drbcc_delete_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type)
{
	return drbcc_delete_file(h, session, type | 0x80000000);
}

DRBCC_RC_t drbcc_delete_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index)
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (drbcc->read_flash_cb || drbcc->erase_flash_cb || drbcc->write_flash_cb)
	{
		return DRBCC_RC_CBREGISTERED;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->session = drbcc_session++;
	*session = drbcc->session;
	if(index & 0x80000000)
	{
		drbcc->curFileType = ((index>>4) & 0xF);
	}
	drbcc->curFileIndex = index;
	drbcc->state = DRBCC_STATE_DELETE_FILE;
	return libdrbcc_request_partition(drbcc);
}

DRBCC_RC_t drbcc_get_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type, const char filename[])
{
	return drbcc_get_file(h, session, type | 0x80000000, filename);
}

DRBCC_RC_t drbcc_get_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index, const char filename[])
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (drbcc->read_flash_cb || drbcc->erase_flash_cb || drbcc->write_flash_cb)
	{
		return DRBCC_RC_CBREGISTERED;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC | OX_BINARY, 0666 );
	if (fd < 0)
	{
		return DRBCC_RC_INVALID_FILENAME;
	}
	close(fd);

	strncpy(drbcc->curFilename, filename, FILENAME_MAX);
	drbcc->session = drbcc_session++;
	*session = drbcc->session;
	if(index & 0x80000000)
	{
		drbcc->curFileType = ((index>>4) & 0xF);
	}
	drbcc->curFileIndex = index;
	drbcc->state = DRBCC_STATE_GET_FILE;
	return libdrbcc_request_partition(drbcc);
}

DRBCC_RC_t drbcc_put_file(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int index, DRBCC_FLASHFILE_TYPES_t type, const char filename[])
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (drbcc->read_flash_cb || drbcc->erase_flash_cb || drbcc->write_flash_cb)
	{
		return DRBCC_RC_CBREGISTERED;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	int fd = open(filename, O_RDONLY | OX_BINARY);
	if (fd < 0)
	{
		return DRBCC_RC_INVALID_FILENAME;
	}
	drbcc->maxFilelength = lseek(fd, 0, SEEK_END);
	close(fd);

	if (drbcc->maxFilelength <= 0)
	{
		return DRBCC_RC_INVALID_FILENAME;
	}

	strncpy(drbcc->curFilename, filename, FILENAME_MAX);
	drbcc->session = drbcc_session++;
	*session = drbcc->session;
	drbcc->curFilelength = 0;
	drbcc->curFileIndex = index;
	drbcc->curFileType = type;
	drbcc->state = DRBCC_STATE_PUT_FILE;

	// 1st read partition
	return libdrbcc_request_partition(drbcc);
}

DRBCC_RC_t drbcc_put_file_type(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int type, const char filename[])
{
	return drbcc_put_file(h, session, type & 0xF, (type>>4) & 0xF, filename);
}

DRBCC_RC_t drbcc_upload_firmware(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, const char *filename)
{
	// FW image has allways index 0 to be used by XMega
	return drbcc_put_file(h, session, 0, DRBCC_FLASHFILE_T_FW_IMAGE, filename);
}

DRBCC_RC_t drbcc_upload_bootloader(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, const char *filename)
{
	// FW image has allways index 0 to be used by XMega
	return drbcc_put_file(h, session, 0, DRBCC_FLASHFILE_T_BOOTLOADER, filename);
}

// put log entry
DRBCC_RC_t drbcc_put_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int ring, int len, uint8_t data[])
{
	int i;
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (len > (DRBCC_MAX_MSG_LEN - 3))
	{
		return DRBCC_RC_MSG_TOO_LONG;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		msg->msg_len = len + 3;
		msg->msg[0] = DRBCC_REQ_PUT_LOG;
		msg->msg[1] = ring;
		msg->msg[2] = len;
		for (i = 0; i < len; i++)
		{
			msg->msg[i + 3] = data[i];

		}

		libdrbcc_add_msg_prio(drbcc, msg);
		if (session)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;
}

DRBCC_RC_t drbcc_get_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, int ring, int entries)
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (drbcc->read_flash_cb || drbcc->erase_flash_cb || drbcc->write_flash_cb)
	{
		return DRBCC_RC_CBREGISTERED;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->logtype = ring;
	drbcc->start = 0;
	drbcc->entries = entries;
	drbcc->state = DRBCC_STATE_GET_LOG;

	if (ring)
	{
		DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

		if (msg != NULL)
		{
			msg->msg_len = 2;
			msg->msg[0] = DRBCC_REQ_RINGLOG_POS;

			libdrbcc_add_msg_prio(drbcc, msg);
		}
		else
		{
			return DRBCC_RC_OUTOFMEMORY;	// 1st read partition
		}
	}

	drbcc->curFileIndex = 0;
	drbcc->session = drbcc_session++;
	*session = drbcc->session;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_get_pos(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
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
		msg->msg[0] = DRBCC_REQ_RINGLOG_POS;
		msg->msg[1] = 0; //Set default

		libdrbcc_add_msg_prio(drbcc, msg);
		if (session && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;	// 1st read partition
}

DRBCC_RC_t drbcc_clear_log(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session)
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
		msg->msg[0] = DRBCC_CLEAR_RINGLOG_REQ;

		libdrbcc_add_msg_prio(drbcc, msg);
		if (session && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;	// 1st read partition
}

DRBCC_RC_t drbcc_debug_set(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int address, int len, uint8_t data[])
{
	DRBCC_t *drbcc = h;
	CHECK_HANDLE(drbcc);

	if (len > (DRBCC_MAX_MSG_LEN - 4))
	{
		return DRBCC_RC_MSG_TOO_LONG;
	}

	if (drbcc->session)
	{
		return DRBCC_RC_SESSIONACTIVE;
	}

	drbcc->state = DRBCC_STATE_USER;

	DRBCC_MESSAGE_t *msg = malloc(sizeof(DRBCC_MESSAGE_t));

	if (msg != NULL)
	{
		int i;
		msg->msg_len = len + 4;
		msg->msg[0] = DRBCC_REQ_DEBUG_SET;
		msg->msg[1] = (uint8_t) ((address >>  8) & 0xFF);
		msg->msg[2] = (uint8_t) ((address >>  0) & 0xFF);
		msg->msg[3] = len;
		for (i = 0; i < len; i++)
		{
			msg->msg[i + 4] = data[i];
		}

		libdrbcc_add_msg_prio(drbcc, msg);
		if (session && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;	// 1st read partition
}

DRBCC_RC_t drbcc_debug_get(DRBCC_HANDLE_t h, DRBCC_SESSION_t *session, unsigned int address)
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
		msg->msg[0] = DRBCC_REQ_DEBUG_GET;
		msg->msg[1] = (uint8_t) ((address >>  8) & 0xFF);
		msg->msg[2] = (uint8_t) ((address >>  0) & 0xFF);

		libdrbcc_add_msg_prio(drbcc, msg);
		if (session && drbcc->session_cb)
		{
			drbcc->session = drbcc_session++;
			*session = drbcc->session;
		}
		return DRBCC_RC_NOERROR;
	}
	return DRBCC_RC_OUTOFMEMORY;	// 1st read partition
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
