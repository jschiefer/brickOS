//
// the main function
//

#include <sched.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <argp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include "rcxtty.h"
#include "transceiver.h"
#include "logger.h"
#include "daemon.h"

#define LOCK_STACK_SIZE 0x20000	// size of locked RAM for stack
#define	LOCK_HEAP_SIZE  0x20000 // size of locked RAM for heap

#define DEFAULT_TTY "/dev/ttyS0"
#define DEFAULT_PORT 7776
#define DEFAULT_MAX_CLIENTS 16
#define EXTRA_WAIT_SLOW  10
#define EXTRA_WAIT_FAST  0

static char* default_tty = DEFAULT_TTY;

// these are read by argp_parse()
const char *argp_program_version = "lnpd 0.9";
const char *argp_program_bug_address = "http://brickos.sourceforge.net/rptrvwbugs.htm";
static char doc[] = "lnpd -- a LinuX Interface to the BrickOS Networking Protocol";

// Used by `main' to communicate with `parse_opt'
struct arguments
{
	int highspeed;
	int extra_wait;
	int realtime;
	int nolock;
	char *tty_device;
	unsigned short port;
	char *logfile;
	int dologging;
	int lflag[LNPD_LOG_MAX];
	int nodaemon;
	int maxclients;
};

// The options we understand.
static struct argp_option options[] =
{
	{"fast",		'f', 0, 0,	"run LNP at 4800 baud" },
	{"extrawait",	'e', "mseconds", 0 , "extra wait time for lnp" },
	{"debug",		'd', 0, 0,	"don´t go into background" },
	{"realtime",	'r', 0, 0,  "run in realtime mode" },
	{"nolock",      'n', 0,	0,	"do not manage tty lockfiles" },
	{"tty",    		't', "tty_device",	0,  "tty device to use, defaults to " DEFAULT_TTY },
	{"port",		'p', "TCP_port", 0, "TCP Port to use, defaults to 7776" },
	{"maxclients",	'm', "number", 0, "max number of clients, default 16" },
	{"log",			'l', "filename", OPTION_ARG_OPTIONAL,
		"enable logging to syslog or file\nfilename '-' means: log to stderr" },
	{"verbosity",	'v', "level", 0,
		"enable enhanced logging, valid levels: d,l,i,a,c" },
	{ 0 }
};

// Parse a single option
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
	/* Get the INPUT argument from `argp_parse', which we
	know is a pointer to our arguments structure. */
	struct arguments *arguments = state->input;
	char *endptr;
	unsigned long ul_value;
	

	switch (key)
	{
	case 'f':
		arguments->highspeed = 1;
		break;
	case 'm':
		ul_value = strtoul(arg,&endptr,0);
		if (*endptr || !ul_value || ul_value > 256)
			argp_failure(state,1,0,"invalid number of clients requested");
		arguments->maxclients = ul_value;
		break;
	case 'e':
		ul_value = strtoul(arg,&endptr,0);
		if (*endptr || ul_value > 100 )
			argp_failure(state,1,0,"invalid extra_wait requested");
		arguments->extra_wait = ul_value;
		break;
	case 'd':
		arguments->nodaemon = 1;
		break;
	case 'r':
		arguments->realtime = 1;
		break;
	case 'n':
		arguments->nolock = 1;
		break;
	case 't':
		arguments->tty_device = arg;
		break;
	case 'p':
		ul_value = strtoul(arg,&endptr,0);
		if (*endptr || !ul_value || ul_value > 0xfffe)
			argp_failure(state,1,0,"invalid TCP port requested");
		arguments->port = ul_value;
		break;
	case 'l':
		arguments->dologging = 1;
		arguments->logfile = arg;
		break;
	case 'v':
		while(*arg)
		{
			switch (tolower(*arg))
			{
				case 'd':
					arguments->lflag[LNPD_LOG_DEBUG] = 1;
					break;
				case 'l':
					arguments->lflag[LNPD_LOG_LOGICAL] = 1;
					break;
				case 'i':
					arguments->lflag[LNPD_LOG_INTEGRITY] = 1;
					break;
				case 'a':
					arguments->lflag[LNPD_LOG_ADDRESSING] = 1;
					break;
				case 'c':
					arguments->lflag[LNPD_LOG_CLIENT] = 1;
					break;
				default:
					argp_failure(state,1,0,"invalid verbosity level requested");
			}
			++arg;
		}
		break;
	case ARGP_KEY_ARG:
		// no arguments accepted
		argp_usage (state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp opt_parser = { options, parse_opt, 0, doc };

// switch to realtime mode, if possible
static int go_realtime(void)
{
	char dummystack[LOCK_STACK_SIZE]; // reserve 64 KByte Stack
	void *dummyheap;
	int rval;
	
	// avoid swapping due to stack increase
	memset(dummystack,0,sizeof(dummystack));
	
	// avoid swapping due to malloc() calls
	dummyheap = malloc(LOCK_HEAP_SIZE);
	if (!dummyheap) error_exit("malloc");
	memset(dummyheap,0,LOCK_HEAP_SIZE);
	
	rval = mlockall( MCL_CURRENT | MCL_FUTURE );
	
	free(dummyheap);
	
	if (!rval)
	{
		// set realtime scheduling	
		struct sched_param schedParams;
		schedParams.sched_priority = sched_get_priority_max(SCHED_FIFO);
		rval = sched_setscheduler(0,SCHED_FIFO,&schedParams);
		if ( !rval ) munlockall();
	}
	return rval;
}

// go into background
void daemonise(void)
{
	pid_t pid;
	
	if ( (pid = fork())< 0)
	{
		perror("fork");
		exit(1);
	}
	else if (pid !=0 ) exit (0);
	
	setsid();
	umask(0);
}

// the signal handler
void signal_handler(int signo)
{
	log(LNPD_LOG_FATAL,"caught signal %s, exiting",strsignal(signo));
	exit(0);
}

// catch Signals
void install_handler(void)
{
    struct sigaction saction;

    sigemptyset(&saction.sa_mask);
    sigaddset(&saction.sa_mask,SIGQUIT);
    sigaddset(&saction.sa_mask,SIGTERM);
    sigaddset(&saction.sa_mask,SIGINT);
    saction.sa_flags = 0;
    saction.sa_handler = signal_handler;
    sigaction(SIGQUIT,&saction,NULL);
    sigaction(SIGTERM,&saction,NULL);
    sigaction(SIGINT,&saction,NULL);
}

// called at exit
void cleanup(void)
{
	tty_exit();
}

int main(int argc, char* argv[])
{
	int rcxfd,i;
	struct arguments option_values;
	char *myname;
	sigset_t blockset,savedset;
	
	myname = strrchr(argv[0],'/');
	if (!myname) myname = argv[0];
	
	// set options defaults
	option_values.highspeed = 0;
	option_values.extra_wait = -1;
	option_values.nodaemon = 0;
	option_values.realtime = 0;
	option_values.nolock = 0;
	option_values.tty_device = default_tty;
	option_values.port = DEFAULT_PORT;
	option_values.logfile = NULL;
	option_values.dologging = 0;
	option_values.maxclients = DEFAULT_MAX_CLIENTS;
	for (i=0;i< LNPD_LOG_MAX;++i) option_values.lflag[i] = 0;
	option_values.lflag[LNPD_LOG_FATAL] = 1;
	option_values.lflag[LNPD_LOG_INFO] = 1;
	
	// parse command line options
    if (argp_parse (&opt_parser, argc, argv, 0, 0, &option_values)) error_exit("argp_parse");

    // check for optimal extra_wait
    if ( option_values.extra_wait < 0 )
    	option_values.extra_wait = option_values.highspeed ? EXTRA_WAIT_FAST : EXTRA_WAIT_SLOW;

    // go into background
    if (!option_values.nodaemon) daemonise();

    // block signals while initialising
    sigemptyset(&blockset);
    sigaddset(&blockset,SIGQUIT);
    sigaddset(&blockset,SIGTERM);
	sigaddset(&blockset,SIGINT);
    sigprocmask(SIG_BLOCK,&blockset,&savedset);

	atexit(cleanup);
	
	// prepare to catch Signals
    install_handler();
	
	// initialise logging
	if ( option_values.dologging )
	{
		log_init(option_values.logfile,myname);
		for (i=0;i< LNPD_LOG_MAX;++i) log_set_level(i,option_values.lflag[i]);
	}
	
	// initialise tty
	rcxfd=tty_init(option_values.highspeed,option_values.nolock,option_values.tty_device);	
	
	// initialise transceiver
	init_transceiver(option_values.highspeed,option_values.extra_wait,rcxfd);
	
	// realtime wanted ?
	if (option_values.realtime)
	{
		if ( !go_realtime())
			log(LNPD_LOG_INFO,"running in Realtime Mode");
		else
		{
			log(LNPD_LOG_FATAL,"can´t go realtime: %s",strerror(errno));
			exit(1);
		}
	}
	else
		log(LNPD_LOG_INFO,"running in Timesharing Mode");
	
	// initialize daemon
	daemon_init(option_values.port,option_values.maxclients);
	
	// unblock signals now
    sigprocmask(SIG_SETMASK,&savedset,NULL);

	// and go
	run_transceiver();
	
	// never reached
	return 0;
}
