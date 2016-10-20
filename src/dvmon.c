/* Editor hints for vim
 * vim:set ts=4 sw=4 noexpandtab:  */

/**\file
 * \brief  dvmon board controller com
 * \author Sascha Dierberg
 * 
 * (C) 2016 DResearch Digital Media Systems GmbH
 *
 * $Id$
 *
 */ 

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <stdio.h>

#include "dvmon.h"

#ifdef HAVE_LIBDRTRACE
#include <drhiptrace.h>
#else
#endif

#include "trace.h"

#define LOCKDIR "/var/lock"
#define TIMEOUT 2


#if USE_OPEN_BINARY
#define OX_BINARY O_BINARY
#else
#define OX_BINARY 0
#endif

int s_fd = -1;
char s_lockfile[256];

// returns < 0 -> negative error code
// fills in the lockfile parameter (devname if liblockfile is available,
// full lockfile path else)
static int libdrbcc_open_tty(const char *dev)
{
	int llen = sizeof(s_lockfile) -1;
	char tty[256];
	char realdev[strlen(dev)+1];
	int fd;
	char *p;
	int baud;
	struct termios tios;
    ssize_t r;
#ifndef HAVE_LIBLOCKDEV
	char pname[256], pidbuf[20];
	int fd1, fd2;
	int pid, len;
	time_t stamp;
#endif
	// determine the real device path in case of a symlink
    r = readlink(dev, realdev, sizeof(realdev));
    if (r < 0) 
	{
        strncpy(realdev, dev, sizeof(realdev));
		realdev[sizeof(realdev)-1] = '\0';
    }
	else
	{
		realdev[((size_t)r > sizeof(realdev) ? sizeof(realdev)-1 : (size_t)r)] = '\0';
	}
	// generate the base name of the tty
	if ((p = rindex(realdev, '/')) != NULL || (p = rindex(realdev, '\\')) != NULL)
	{
		strncpy(tty, p+1, sizeof(tty));
	}	
	else
	{
		strncpy(tty, realdev, sizeof(tty));
	}
	tty[sizeof(tty)-1] = '\0'; // trailing \0
#ifdef HAVE_LIBLOCKDEV
	// return the device name
	snprintf(s_lockfile, llen, "%s", tty);
	s_lockfile[llen - 1] = '\0';
	if (0 != dev_lock(s_lockfile))
	{
			return -1; // we give up
	}
#else
	mkdir(LOCKDIR, 0777);
	// return the lock file name
	snprintf(s_lockfile, llen, "%s/LCK..%s", LOCKDIR, tty);
	s_lockfile[llen - 1] = '\0';
	// time stamp for timeout estimation
	stamp = time(NULL);
	// try to lock the tty during the timeout period
	for (;;)
	{
		if (time(NULL) > stamp + TIMEOUT)
		{
			return -1; // we give up
		}
		// try to open AND create the lockfile
		if ((fd1 = open(s_lockfile, O_RDWR|O_CREAT|O_EXCL, 0666)) >= 0)
		{
			break; // we succeeded
		}
		usleep(100000); // another one locked the tty, so be patient
		if((fd1 = open(s_lockfile, O_RDWR|O_EXCL, 0666)) < 0)
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
		unlink(s_lockfile);
	}
	// generate the lock file content
	sprintf(pname, "%10d\n", getpid());
	len = strlen(pname);
	// write the lock file
	if (write(fd1, pname, len) != len)
	{
		return -2;
	}
	close(fd1);
#endif
	// now open the tty
	// fd = open(dev, O_RDWR | O_NOCTTY | O_NDELAY);
	fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK | O_NDELAY);
	if (fd < 0)
	{
#ifdef HAVE_LIBLOCKDEV
		dev_unlock(s_lockfile, 0);
#else
		unlink(s_lockfile);
#endif
		return -2;
	}
	if (tcgetattr(fd, &tios) < 0)
	{
#ifdef HAVE_LIBLOCKDEV
		dev_unlock(s_lockfile, 0);
#else
		unlink(s_lockfile);
#endif
		close(fd);
		return -2;
	}
	cfmakeraw(&tios);
	baud = B921600;

	if (cfsetospeed(&tios, baud) < 0)
	{
#ifdef HAVE_LIBLOCKDEV
		dev_unlock(s_lockfile, 0);
#else
		unlink(s_lockfile);
#endif
		close(fd);
		return -2;
	}
	tios.c_cflag &= ~CSTOPB; // 1 stop bit
	if (tcsetattr(fd, TCSANOW, &tios) < 0)
	{
#ifdef HAVE_LIBLOCKDEV
		dev_unlock(s_lockfile, 0);
#else
		unlink(s_lockfile);
#endif
		close(fd);
		return -2;
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

int dvmon_init(const char* dev)
{
	s_fd = libdrbcc_open_tty(dev);
	if (s_fd < 0)
	{
		return -1;
	}	
	return 0;
}

#define FIRMWARE_SECTION_SIZE 16384

static unsigned int crc16_update(unsigned int  crc, unsigned char a)
{
	int i;

	crc ^= a;
	for (i = 0; i < 8; ++i)
	{
	    if (crc & 1)
		crc = (crc >> 1) ^ 0xA001;
	    else
		crc = (crc >> 1);
	}

	return crc;
}

static unsigned int calcCRC16(unsigned char *f, unsigned int size)
{
	unsigned char buf[FIRMWARE_SECTION_SIZE];
	unsigned int i, crc16;

	for (i = 0; i < FIRMWARE_SECTION_SIZE; i++) 
		buf[i] = 0xff;

	crc16 = 0;

	memcpy(buf, f, size);
	
	for (i = 0; i < FIRMWARE_SECTION_SIZE; i++) 
		crc16 = crc16_update(crc16, buf[i]); // calc crc16 over complete section, unused space filled with 0xff

	return crc16;
}


static unsigned int waitReadAndCut(int fd, unsigned char* p, int size)
{
	int ret = 0, i = 0;
	
	while(!ret)
	{
		usleep(20000);
		ret = read(fd, p, size);
		i++;
	}
	// TRACE(TL_INFO, "read tries: %i", i);
	p[ret - 1] = 0;
	return ret;
}	

int dvmon(const char* cmd)
{
	int ret;
	char s[320];
	
	if (cmd && cmd[0])
	{
		if (0 == strncmp(cmd, "proto", strlen("proto")))	
		{
			sprintf(s, "V\n");
			ret = write(s_fd, s, strlen(s));
			if (ret != strlen(s))
				TRACE(TL_INFO, "write failed: %s", cmd);			
		}
		else if (0 == strncmp(cmd, "heartbeat", strlen("heartbeat")))	
		{
			int i;
			if (1 == sscanf(cmd, "heartbeat %u", &i))
			{
				sprintf(s, "H %u\n", i);
				ret = write(s_fd, s, strlen(s));
				if (ret == strlen(s))
					puts(s);
			}
		}
		else if (0 == strncmp(cmd, "shutdown", strlen("shutdown")))	
		{
			int i;
			if (1 == sscanf(cmd, "shutdown %u", &i))
			{
				sprintf(s, "H %u\n", i);
				ret = write(s_fd, s, strlen(s));
				if (ret == strlen(s))
					puts(s);
			}
		}
		else if (0 == strncmp(cmd, "waitpoweroff", strlen("waitpoweroff")))	
		{			
			sprintf(s, "R 0\n");
			ret = write(s_fd, s, strlen(s));
			if (ret == strlen(s))
				puts(s);
		}
 		else if (0 == strncmp(cmd, "getrtc", strlen("getrtc")))	
		{
			puts("rtc_cb: 2000-01-01 00:00:00 (epoch=0)");
			return 0;
		}
 		else if (0 == strncmp(cmd, "pfiletype", strlen("pfiletype")))	
		{			
			// cmd=="pfiletype 0x50,/etc/hydraip-devid"			
			unsigned int type = (unsigned int)-1;
			char path[FILENAME_MAX] = "";
			if(2 != sscanf(cmd, "%*s%i,%s", &type, path) && (0x50 != type)) // devid only
			{
				TRACE(TL_INFO, "command syntax problem: %s", cmd);
				return 1;
			}
			else
			{
				char f[320];
				int fd = open(path, O_RDONLY | OX_BINARY);
				if (fd < 0)
				{
					TRACE(TL_INFO, "open file failed: %s", path);
					return 2;
				}
				ret = read(fd, f, sizeof(f));
				if (ret <= 0)
				{
					TRACE(TL_INFO, "read file failed: %s", path);
					close(fd);
					return 2;
				}
				
				for (ret--; ret>=0; ret--)
				{
					if ('\n' == f[ret])
						f[ret] = '|';
				}
				close(fd);			
				sprintf(s, "CW 0 %s\n", f); 
				ret = write(s_fd, s, strlen(s));
				if (ret == strlen(s))
					puts(s);
			}
		}
 		else if (0 == strncmp(cmd, "gfiletype", strlen("gfiletype")))	
		{			
			// cmd=="gfiletype 0x50,/etc/hydraip-devid"
			unsigned int type = (unsigned int)-1;
			char path[FILENAME_MAX] = "";
			if(2 != sscanf(cmd, "%*s%i,%s", &type, path) && (0x50 != type)) // devid only
			{
				TRACE(TL_INFO, "command syntax problem: %s", cmd);
				return 1;
			}
			else
			{	
				char f[320];
				sprintf(s, "CR 0\n");
				ret = write(s_fd, s, strlen(s));
				if (ret == strlen(s))
				{
					puts(s);
					ret = waitReadAndCut(s_fd, f, sizeof(f));
					if (ret <= 0)
					{
						TRACE(TL_INFO, "read bcc failed: %s", cmd);
						return 4;
					}
					puts(f);
					for (ret--; ret>0; ret--)
					{
						if ('|' == f[ret])
							f[ret] = '\n';
					}
					int fd = open(path, O_RDWR | O_CREAT | O_TRUNC | OX_BINARY, 0666);
					if (fd < 0)
					{
						TRACE(TL_INFO, "open file failed: %s", path);
						return 2;
					}					
					// cut trailing "cr 0 " and \n
					ret = write(fd, f+5, strlen(f)-7);					
					close(fd);
					if (ret != strlen(f)-7)
					{
						TRACE(TL_INFO, "write file failed: %s", path);
						return 2;
					}					
					return 0;
				}
				else
				{
					TRACE(TL_INFO, "write failed: %s", cmd);
					return 3;
				}				
			}
		}
 		else if (0 == strncmp(cmd, "fwupload", strlen("fwupload")))	
		{			
			char crc[32] = "";
			char path[FILENAME_MAX] = "";
			if (1 != sscanf(cmd, "%*s %s", path)) // devid only
			{
				TRACE(TL_INFO, "command syntax problem: %s", cmd);
				return 1;
			}
			else
			{
				int i, size;
				char f[FIRMWARE_SECTION_SIZE];
				int fd = open(path, O_RDONLY | OX_BINARY);
				if (fd < 0)
				{
					TRACE(TL_INFO, "open file failed: %s", path);
					return 2;
				}
				size = read(fd, f, sizeof(f));
				if (size <= 0)
				{
					TRACE(TL_INFO, "read file failed: %s", path);
					close(fd);
					return 2;
				}				
				close(fd);			

				for (i = 0; i < size; i += 128) // lines 
				{
					int j;
					char s[400]; 					
					sprintf(s, "P %04X ", i);

					for (j = 0; j < 128 && i + j < size; j++) 
					{				
						sprintf(s + strlen(s), "%02X ", f[i + j]);						
					}
					puts(s);
					sprintf(s + strlen(s), "\n");
					
					ret = write(s_fd, s, strlen(s));
					if (ret != strlen(s))
					{
						TRACE(TL_INFO, "write bcc failed: %s", s);
						return 5;
					}					
					ret = waitReadAndCut(s_fd, s, sizeof(s));
					puts(s);
				}
				
				ret = write(s_fd, "Q\n", 2);				
				if (ret != 2)
				{
					TRACE(TL_INFO, "write bcc failed: %s", s);
					return 6;
				}	
				ret = waitReadAndCut(s_fd, s, sizeof(s));
				puts(s);
				
				sprintf(crc, "q %04X", calcCRC16(f, size));
				
				if (0 == strncmp(s, crc, 6))
				{	
					sprintf(s, "U %04X\n", calcCRC16(f, size));
					ret = write(s_fd, s, strlen(s));
					if (ret != strlen(s))
					{
						TRACE(TL_INFO, "write bcc failed: %s", s);
						return 7;
					}	
					
					ret = waitReadAndCut(s_fd, s, sizeof(s));
					if (0 == strncmp(s, "u ok", 4))
					{
						puts(s);	
						return 0;
					}
				}
				TRACE(TL_INFO, "update failed crc <%s> ", crc);
				return 7;
			}
		}
		else
		{
		    TRACE(TL_INFO, "Unknown cmd <%s>\n", cmd);	
			return 1;
		}
		ret = waitReadAndCut(s_fd, s, sizeof(s));
		puts(s);	
	}
	return 0;
}

void dvmon_term()
{
	close(s_fd);
#ifdef HAVE_LIBLOCKDEV
	dev_unlock(s_lockfile, 0);
#else
	unlink(s_lockfile);
#endif
	return;
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
