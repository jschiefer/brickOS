/*
 *  The Original Code is legOS code, released October 2, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s): Kekoa Proudfoot  <kekoa@graphics.stanford.edu>
 */

#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "logger.h"
#include "lnp.h"
#include "rcxtty.h"

static int fd;
static char *lockfilename;

// try to create lockfile
// returns -1 on failure, 0 on success
static int make_lockfile(const char *device)
{
	char lockname[FILENAME_MAX];
	int nchars,result,lockfd,lockpid;
	FILE* lockfp;
    char filepid[16];
	
	// create lockfile name
    if ( strchr( device, '/' ) != NULL )
        device = strrchr( device, '/' ) +1;
    nchars = snprintf( lockname, FILENAME_MAX, "%s/%s%s" , LOCKDIR, LOCKNAME, device);
    // should work with glibc2.0 and 2.1
    if (nchars < 0 || nchars >= FILENAME_MAX)
    {
    	logmsg(LNPD_LOG_INFO,"tty name too long");
    	return -1;
    }
	
	// create lockfile
	while ( ( lockfd = open(lockname, O_WRONLY | O_CREAT | O_EXCL, 0644) ) < 0 )
	{
	    if (errno != EEXIST )
		{
			logmsg(LNPD_LOG_INFO,"cannot create lockfile: %s", strerror(errno));
 			return -1;
		}
		
		else // lock file already there
		{
			// read pid from Lockfile
			if ( ( lockfp = fopen(lockname, "r" )) == NULL )
			{
				if ( errno == ENOENT ) continue; // lock file disappeared
				else
				{
					logmsg(LNPD_LOG_INFO,"cannot open lockfile %s",lockname);
					return -1;
				}
			}
			result = fscanf(lockfp, "%d", &lockpid);
			fclose( lockfp );
			
			if (result != 1)
			{
				logmsg(LNPD_LOG_INFO,"cannot read pid from lockfile %s",lockname);
				return -1;
			}
			
	    	// check if process already dead or it´s our own pid
	    	result = kill(lockpid, 0);
	    	if ( (result < 0 && errno == ESRCH) || ( lockpid == (int)getpid() ) )
		    {
				// process is gone, try to remove stale lock
				logmsg( LNPD_LOG_INFO, "trying to unlink stale lockfile");
				if ( unlink(lockname) < 0 &&
			         errno != EINTR && errno != ENOENT )
				{
			    	logmsg( LNPD_LOG_INFO, "cannot unlink stale lockfile: %s",strerror(errno));
			    	return -1;
				}
				continue;
		    }
		
		    logmsg(LNPD_LOG_INFO, "device is locked by pid %d ",lockpid);
			return -1;
		}
	}
	
	lockfilename = strdup(lockname);
	
	// write pid to lockfile
    sprintf( filepid, "%10d\n", (int) getpid() );
    if ( write(lockfd, filepid, strlen(filepid)) != strlen(filepid) )
	{
		logmsg(LNPD_LOG_INFO,"cannot write to lockfile %s",strerror(errno));
    	close(lockfd);
 		return -1;
	}

    close(lockfd);
	logmsg(LNPD_LOG_INFO, "created lock file %s",lockname);
	return 0;
}

void tty_exit(void)
{
	if (lockfilename) unlink(lockfilename);
}	

// initialize RCX communications port
int tty_init(int highspeed,int nolock,const char *device)
{
	struct termios ios;
    struct serial_struct ttyinfo;

    // try to create lockfile
    if ( !nolock && make_lockfile(device))
   	{
   		logmsg(LNPD_LOG_FATAL,"cannot create lockfile");
   		exit(1);
	}    	

	// open the device
	if ( (fd = open(device, O_RDWR )) < 0)
 		error_exit("open");

	// check if it´s a tty
	if (!isatty(fd))
	{
    	logmsg(LNPD_LOG_FATAL, "%s is not a tty", device);
    	exit(1);
	}
	
	// set to noncanonical mode, implies nonblocking behaviour
	memset(&ios, 0, sizeof(ios));
  	ios.c_cflag = CREAD | CLOCAL | CS8 | (highspeed ? 0 : PARENB | PARODD);
	cfsetispeed(&ios, highspeed ? LNP_BAUD_FAST : LNP_BAUD_SLOW );
	cfsetospeed(&ios, highspeed ? LNP_BAUD_FAST : LNP_BAUD_SLOW );
  	if (tcsetattr(fd, TCSANOW, &ios) == -1)
    	error_exit("tcsetattr");
    	
	// try to set to "hard realtime mode"
    if  (ioctl(fd,TIOCGSERIAL,&ttyinfo))
    	error_exit("ioctl[TIOCGSERIAL]");
    	
    switch (ttyinfo.type)
    {
	   	case PORT_16550A:
			ttyinfo.type = PORT_16450;
    		ttyinfo.flags |= ASYNC_LOW_LATENCY;
	   		break;
	   	case PORT_16450:
    	case PORT_8250:
    	case PORT_16550:
    		ttyinfo.flags |= ASYNC_LOW_LATENCY;
    		break;
	   	default:
  			logmsg(LNPD_LOG_INFO,"don´t know how to configure tty, trying low_latency");
    		ttyinfo.flags |= ASYNC_LOW_LATENCY;
    }
	
	if (ioctl(fd,TIOCSSERIAL,&ttyinfo))
	{
		logmsg(LNPD_LOG_INFO,"failed to configure tty: %s",strerror(errno));
	}
	
	// we must close and reopen the tty
	close(fd);
	if ( (fd = open(device, O_RDWR )) < 0)
 		error_exit("open");
	
    return fd;
}
