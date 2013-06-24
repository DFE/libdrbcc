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

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "drbcc_sema.h"
#include "trace.h"

struct _drbcc_sema
{
	int readfd;
	int writefd;
};

int drbcc_sema_create(DRBCC_sema_t *sema)
{
	int pfd[2];

	TRACE(TL_DEBUG, "sema create()");
	*sema = malloc(sizeof(struct _drbcc_sema));
	if(0 == sema)
	{
		TRACE(TL_INFO, "sema create() malloc failed!");
		return -1;
	}

	if(pipe(pfd) == -1)
	{
		free(*sema);
		*sema = 0;
		TRACE(TL_INFO, "sema create() pipe failed!");
		return -1;
	}
	(*sema)->writefd = pfd[1];
	(*sema)->readfd  = pfd[0];
	return 1;

}

int drbcc_sema_release(DRBCC_sema_t sema)
{
	int wr = 0;
	int count = 0;

	if(0 == sema)
	{
		TRACE(TL_INFO, "sema release() sema was closed!");
		return -1;
	}

	while(wr != 1)
	{
		++count;
		wr = write(sema->writefd, &wr, 1);
		if(-1 == wr)
		{
			if(EPIPE == errno)
			{
				TRACE(TL_INFO, "sema release() reading end closed!");
				return -1;
			}
			if(count < 1000)
			{
				TRACE(TL_INFO, "sema release() error: %s! continue", strerror(errno));
				continue;
			}
			TRACE(TL_INFO, "sema release() error: %s! return -1", strerror(errno));
			return -1;
		}
		TRACE(TL_TRACE, "sema release() write returned %d!", wr);
	}
	TRACE(TL_DEBUG, "sema release() ok: %d", sema->writefd);
	return 1;
}

int drbcc_sema_wait(DRBCC_sema_t sema)
{
	ssize_t rd = 0;;
	char c_buffer;

	if(0 == sema)
	{
		TRACE(TL_INFO, "sema wait() sema was closed already!");
		return -1;
	}

	while(1 != rd)
	{
		rd = read(sema->readfd, &c_buffer, 1);
		if(0 == rd)
		{
			TRACE(TL_INFO, "sema wait() sema was closed!");
			return -1;
		}
		if(-1 == rd)
		{
			if(EINTR == errno)
			{
				continue;
			}
			TRACE(TL_INFO, "sema wait() read failure. errno %d! %s", errno, strerror(errno));
			return -1;
		}
	}
	TRACE(TL_DEBUG, "sema wait() ok: %d", sema->readfd);
	return 1;
}

void drbcc_sema_destroy(DRBCC_sema_t *sema)
{
	TRACE(TL_DEBUG, "sema destroy");
	if((0 != sema) && (0 != *sema))
	{
		if((*sema)->writefd >= 0)
		{
			TRACE(TL_DEBUG, "Close write sema: %d", (*sema)->writefd);
			close((*sema)->writefd);
			(*sema)->writefd = -1;
		}
		if((*sema)->readfd >= 0)
		{
			TRACE(TL_DEBUG, "Close read sema: %d", (*sema)->readfd);
			close((*sema)->readfd);
			(*sema)->readfd = -1;
		}
		free(*sema);
		*sema = 0;
	}
}
