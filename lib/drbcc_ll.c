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
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <sys/timeb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>

#ifdef HAVE_LIBDRTRACE
#include <drhiptrace.h>
#else
#include "drbcc_trace.h"
#endif
#include "drbcc.h"
#include "drbcc_ll.h"
#include "drbcc_trace.h"

#define TRACE_NAME "libdrbcc"  /**< trace channel name */

#ifdef HAVE_LIBDRTRACE
static DRHIP_TRACE_UNIT(ldrbcc);
static int trace_category = DRHIP_TCAT_0;
#endif

int libdrbcc_initialized = 0;

void libdrbcc_free_queue(DRBCC_MESSAGE_t *msg);

#define LOCKDIR "/tmp"
#define TIMEOUT 2

#ifdef HAVE_LIBDRTRACE
drhip_trace_unit_t drbcc_trace_unit()
{
	return ldrbcc;
}
#endif

DRBCC_RC_t drbcc_set_tracemask(unsigned long tracemask)
{
#ifdef HAVE_LIBDRTRACE
		if (!DRHIP_TRACE_IS_UNKNOWN(ldrbcc))
		{
		DRHIP_TRACE_SET_MASK(ldrbcc, tracemask);
		}
#else
	libdrbcc_set_tracemask(tracemask);
#endif
	return	DRBCC_RC_NOERROR;
}

static void autoTraceInit()
{
#ifdef HAVE_LIBDRTRACE
		if (DRHIP_TRACE_IS_UNKNOWN(ldrbcc))
		{
				DRHIP_TRACE_REGISTER_MODULE(ldrbcc, TRACE_NAME);
				DRHIP_TRACE_SET_LVL(ldrbcc, trace_category, DRHIP_TLVL_8);
		}
#endif
}

// returns < 0 -> negative error code
// fills in the lockfile path
static int libdrbcc_open_tty(const char *dev, DRBCC_BR_t br, char *lockfile, int llen)
{
	char pname[256], tty[256], pidbuf[20];
	int fd, fd1, fd2;
	time_t stamp;
	int pid, len;
	char *p;
	int baud;
	struct termios tios;

	mkdir(LOCKDIR, 0777);

	// generate the base name of the tty
	if ((p = rindex(dev, '/')) != NULL || (p = rindex(dev, '\\')) != NULL)
	{
		strncpy(tty, p+1, sizeof(tty));
	}	
	else
	{
		strncpy(tty, dev, sizeof(tty));
	}

	tty[sizeof(tty)-1] = '\0'; // trailing \0

	// return the lock file name
	snprintf(lockfile, llen, "%s/LCK..%s", LOCKDIR, tty);
	lockfile[llen - 1] = '\0';

	// time stamp for timeout estimation
	stamp = time(NULL);

	// try to lock the tty during the timeout period
	for (;;)
	{
		if (time(NULL) > stamp + TIMEOUT)
		{
			return -DRBCC_RC_DEVICE_LOCKED; // we give up
		}

		// try to open AND create the lockfile
		if ((fd1 = open(lockfile, O_RDWR|O_CREAT|O_EXCL, 0666)) >= 0)
		{
			break; // we succeeded
		}

		usleep(100000); // another one locked the tty, so be patient

		if((fd1 = open(lockfile, O_RDWR|O_EXCL, 0666)) < 0)
		{
			continue; // we can not open the lock file, is it still there?
		}

		// get the pid from the lock file
		if (read(fd1, pidbuf, sizeof(pidbuf)) < 0)
		{
			close(fd1);

			continue; // we can't read the file, try again
		}

		pid = atoi(pidbuf);
		sprintf(pname, "/proc/%d", pid);

		if ((fd2 = open(pname, O_RDONLY, 0666)) >= 0)
		{
			close(fd2);
			close(fd1);

			continue; // the process still exists, so continue
		}

		close(fd1);

		// remove the lock file and try again
		unlink(lockfile);
	}

	// generate the lock file content
	sprintf(pname, "%10d\n", getpid());

	len = strlen(pname);

	// write the lock file
	if (write(fd1, pname, len) != len)
	{
		return -DRBCC_RC_SYSTEM_ERROR;
	}

	close(fd1);

	// rene
	// now open the tty
	//fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);

	if (fd < 0)
	{
		unlink(lockfile);

		return -DRBCC_RC_SYSTEM_ERROR;
	}

	if (tcgetattr(fd, &tios) < 0)
	{
		unlink(lockfile);
		close(fd);

		return -DRBCC_RC_SYSTEM_ERROR;
	}

	cfmakeraw(&tios);

	baud = B921600;

	if (br == DRBCC_BR_115200)
	{
		baud = B115200;
	}
	else if (br == DRBCC_BR_57600)
	{
		baud = B57600;
	}

	if (cfsetospeed(&tios, baud) < 0)
	{
		unlink(lockfile);
		close(fd);

		return -DRBCC_RC_SYSTEM_ERROR;
	}

	tios.c_cflag &= ~CSTOPB; // 1 stop bit

	if (tcsetattr(fd, TCSANOW, &tios) < 0)
	{
		unlink(lockfile);
		close(fd);

		return -DRBCC_RC_SYSTEM_ERROR;
	}





	struct termios ttystate;

    // get the terminal state
	tcgetattr(fd, &ttystate);

	int oldstat = fcntl(fd, F_GETFL, 0 );
	fcntl(fd, F_SETFL, oldstat | O_NONBLOCK );

	// turn off canonical mode
	ttystate.c_lflag &= ~ICANON;
	// minimum number of characters for non-canonical read
	ttystate.c_cc[VMIN] = 0;
	// timeout in deciseconds for non-canonical read
	ttystate.c_cc[VTIME] = 0;

    // set the terminal attributes.
	tcsetattr(fd, TCSANOW, &ttystate);

	return fd;
}

DRBCC_RC_t drbcc_init(unsigned long tracemask)
{
	if (libdrbcc_initialized)
	{
		return DRBCC_RC_WRONGSTATE;
	} 
	else
	{
		libdrbcc_initialized = 1;
		autoTraceInit();
#ifdef HAVE_LIBDRTRACE
		DRHIP_TRACE_SET_MASK(ldrbcc, tracemask);
#else
		libdrbcc_set_tracemask(tracemask);
#endif
		TRACE(DRBCC_TR_PARAM, "tracemask set to 0x%08lX", tracemask);

		return	DRBCC_RC_NOERROR;
	}	
}

DRBCC_RC_t drbcc_term()
{
	if (!libdrbcc_initialized)
	{
		return DRBCC_RC_WRONGSTATE;
	}
	else
	{
		libdrbcc_initialized = 0;
		return	DRBCC_RC_NOERROR;
	}
}

DRBCC_RC_t drbcc_open(DRBCC_HANDLE_t* h)
{
	if (!libdrbcc_initialized)
	{
		return DRBCC_RC_WRONGSTATE;
	}

	DRBCC_t *drbcc = malloc(sizeof(DRBCC_t));

	memset(drbcc, 0, sizeof(DRBCC_t));

	drbcc->ackTimeout.tv_sec = 0;
	drbcc->ackTimeout.tv_usec = 40*1000; // ms in asynch mode
	drbcc->nextTimeout.tv_sec = 1;
	drbcc->nextTimeout.tv_usec = 0; // 1 s 

	drbcc->last_error = DRBCC_RC_NOERROR;
	drbcc->fd = -1;
	drbcc->send_toggle = 0;
	drbcc->expected_recv_toggle = 0;
	drbcc->prioQueue = NULL;
	drbcc->secQueue = NULL;
	drbcc->magic = 0xDAD1DADA;
	drbcc->bufstart = 0;
	drbcc->bufend = 0;

	*h = drbcc;

	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_start(DRBCC_HANDLE_t h, void *context, const char *tty, DRBCC_BR_t br)
{
	DRBCC_t *drbcc = h;
	int fd;
	//int oflags;
	//struct sigaction sa, osa;

	CHECK_HANDLE(drbcc);

	if ((fd = libdrbcc_open_tty(tty, br, drbcc->lockfile, sizeof(drbcc->lockfile))) < 0)
	{
		return DRBCC_RC_DEVICE_LOCKED;
	}

	/*fcntl(fd, F_SETOWN, getpid());

	oflags = fcntl(fd, F_GETFL);
	//fcntl(fd, F_SETFL, oflags | FASYNC);
	fcntl(fd, F_SETFL, oflags | O_ASYNC);

	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGIO);
	sa.sa_flags = 0;
	sa.sa_handler = input_handler;
	sigaction(SIGIO, &sa, &osa);

	// signal(SIGIO, &input_handler);*/

	/*if(fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) < 0)
	return DRBCC_RC_SYSTEM_ERROR;*/

	drbcc->fd = fd;
	drbcc->context = context;

	/* The first sync request is send without a session,
	   no session callback may be called
	*/
	drbcc->wait_for_first_sync_ack = 1;
	drbcc_sync(drbcc, 0);

	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_stop(DRBCC_HANDLE_t h)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	drbcc->error_cb = NULL;
	drbcc->protocol_cb = NULL;
	drbcc->id_cb = NULL;
	drbcc->erase_flash_cb = NULL;
	drbcc->read_flash_cb = NULL;
	drbcc->write_flash_cb = NULL;
	drbcc->flash_id_cb = NULL;
	drbcc->rtc_cb = NULL;
	drbcc->status_cb = NULL;
	drbcc->hdOffRequest_cb = NULL;
	return DRBCC_RC_NOERROR;
}

DRBCC_RC_t drbcc_close(DRBCC_HANDLE_t h)
{
	DRBCC_t *drbcc = h;

	CHECK_HANDLE(drbcc);

	libdrbcc_free_queue(drbcc->secQueue);
	libdrbcc_free_queue(drbcc->prioQueue);

	if (drbcc->fd >= 0)
	{
		close(drbcc->fd);
		unlink(drbcc->lockfile);
	}
	free(drbcc);
	return DRBCC_RC_NOERROR;
}

const char* drbcc_get_error_string(DRBCC_RC_t rc)
{
	switch(rc)
	{
	case DRBCC_RC_NOERROR:				return "DRBCC: No error";
	case DRBCC_RC_SYSTEM_ERROR:			return "DRBCC: System error";
	case DRBCC_RC_UNSPEC_ERROR:			return "DRBCC: Unspecified error";
	case DRBCC_RC_NO_STANDBY_POWER:		return "DRBCC: No standby power";
	case DRBCC_RC_MISSING_START_CHAR:	return "DRBCC: Missing start character";
	case DRBCC_RC_MSG_TOO_LONG:			return "DRBCC: Message too long";
	case DRBCC_RC_UNEXP_START_CHAR:		return "DRBCC: unexpected start character";
	case DRBCC_RC_MSG_TOO_SHORT:		return "DRBCC: Message too short";
	case DRBCC_RC_MSG_CRC_ERROR:		return "DRBCC: Message CRC error";
	case DRBCC_RC_DEVICE_LOCKED:		return "DRBCC: Device locked";
	case DRBCC_RC_MSG_TIMEOUT:			return "DRBCC: Message timeout";
	case DRBCC_RC_OUTOFMEMORY:			return "DRBCC: Out of memory";
	case DRBCC_RC_WRONGSTATE:			return "DRBCC: Wrong state";
	case DRBCC_RC_NOTINITIALIZED:		return "DRBCC: Not libdrbcc_initialized";
	case DRBCC_RC_INVALID_HANDLE:		return "DRBCC: Invalid Handle";
	case DRBCC_RC_CBREGISTERED:			return "DRBCC: Callbacks registered";
	case DRBCC_RC_INVALID_FILENAME:		return "DRBCC: Invalid filename";
	case DRBCC_RC_SESSIONACTIVE:		return "DRBCC: Session active";
	default:							return "DRBCC: Unknown error";
	}	
}


void drbcc_dump_message(FILE *fp, const DRBCC_MESSAGE_t *mesg)
{
	if (mesg->msg_len > 0)
	{
		if (mesg->msg[0] & TOGGLE_BITMASK)
		{
			fprintf(fp, "DRBCC Toogle 1:\n");
		}
		else
		{
			fprintf(fp, "DRBCC Toogle 0:\n");
		}
	}
	else
	{
		fprintf(fp, "DRBCC-Message Length 0!\n");
	}
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
