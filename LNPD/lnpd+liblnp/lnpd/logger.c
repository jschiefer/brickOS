//
// logging stuff
//

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logger.h"

static char *logstr[LNPD_LOG_MAX] =
{
	"Fatal",
	"Debug",
	"Info",
	"Logical",
	"Integrity",
	"Addressing",
	"Client",
};

static int log_enabled[LNPD_LOG_MAX];
static struct timeval starttime;
static int logfd;
static int use_syslog;
static int log_on;

void log_init(char *filename,char *progname)
{
	gettimeofday(&starttime,NULL);
	log_on = 1;
	
	if (!filename)
	{
		use_syslog = 1;
		openlog( progname,LOG_NDELAY,LOG_DAEMON);
	}
	else
	{
		if (!strcmp(filename,"-")) logfd = STDERR_FILENO;
		else
		{
			logfd = open(filename,O_CREAT | O_APPEND | O_WRONLY, 0644);
			if (logfd < 0)
			{
				fprintf(stderr,"cannot open logfile %s: %s\n",filename,strerror(errno));
				exit(1);
			}
		}
	}
}	

void log_set_level(int level,int enabled)
{
	if (level < LNPD_LOG_MAX) log_enabled[level] = enabled;
}

void logmsg(int level, char* format,...)
{
	struct timeval now,diff;
	unsigned long runtime;
	char message[256];
	va_list arglist;
	
	if ( !log_on || level >= LNPD_LOG_MAX || !log_enabled[level]) return;
	va_start(arglist,format);
	gettimeofday(&now,NULL);
	timersub(&now,&starttime,&diff);
	runtime = diff.tv_sec * 1000 + diff.tv_usec / 1000;

	sprintf(message,"%10lu:%s > ",runtime,logstr[level]);
	vsprintf(message+strlen(message),format,arglist);
	
	if (use_syslog)
		syslog(LOG_DAEMON | LOG_INFO,"%s",message);
	else
	{	
		sprintf(message+strlen(message),"\n");
		if ( write(logfd,message,strlen(message)) != strlen(message) )
		{
			fprintf(stderr,"cannot write to logfile %s\n",strerror(errno));
			exit(1);
		}
	}
}
	
	
