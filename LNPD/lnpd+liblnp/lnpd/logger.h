//
// logging stuff
//

#ifndef LOGGER_H
#define LOGGER_H

#include <string.h>
#include <errno.h>

enum loglevel
{
	LNPD_LOG_FATAL,
	LNPD_LOG_DEBUG,
	LNPD_LOG_INFO,
	LNPD_LOG_LOGICAL,
	LNPD_LOG_INTEGRITY,
	LNPD_LOG_ADDRESSING,
	LNPD_LOG_CLIENT,
	LNPD_LOG_MAX
};

extern void log(int level, char* format,...);
extern void log_init(char *filename,char* progname);
extern void log_set_level(int level,int enabled);

#define error_exit(message) \
	{ log(LNPD_LOG_FATAL,"%s.  %s(),%s,line %d",strerror(errno),message,__FILE__,__LINE__); \
	exit(1); }

#endif
	
