//
// the rcx transceiver
//

#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <linux/serial.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "transceiver.h"
#include "logger.h"
#include "lnp.h"
#include "rcxtty.h"
#include "daemon.h"

#define RCV_BUFFER_SIZE 16
#define LOOP_TIMEOUT 10
#define KEEP_ALIVE_TIMEOUT 3500
#define KEEP_ALIVE_URGENT  1000
#define KEEP_ALIVE_BYTE 0xff

#define MAX(a,b) ( (a) > (b) ? (a) : (b) )

static int active;
static int rcxfd;
static unsigned char tx_buffer[MAX_LNP_PACKET];
static unsigned char* tx_next;
static unsigned char* tx_verify;
static unsigned char* tx_end;
static struct serial_icounter_struct tty_error_info;
static struct timeval inter_byte_time, keep_alive_time, tx_allowed_time;
static const unsigned char keepalive_byte = KEEP_ALIVE_BYTE;

// convert a time in milliseconds to struct timeval
static void msecs2timeval(int msecs, struct timeval *tval)
{
	tval->tv_sec = msecs / 1000;
	tval->tv_usec = msecs*1000 % 1000000;
}

// sleep for some milliseconds
static void msleep(unsigned msecs)
{
	struct timeval wtime,now,end;
	
	if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
	msecs2timeval(msecs,&wtime);
	
	timeradd(&now,&wtime,&end);
	
	do
	{
		int result = select(0,NULL,NULL,NULL,&wtime);
		if ( result < 0 && errno != EINTR ) error_exit("select()");
		if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
		if (timercmp(&now,&end,>)) break;
		timersub(&end,&now,&wtime);
	} while (1);
}	

// set a struct timeval to the current time + some milliseconds
// never decrease the timeval
static void set_time(struct timeval *timeset, int msecs)
{
	struct timeval now,diff,new;
	
	msecs2timeval(msecs,&diff);
	if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
	timeradd(&now,&diff,&new);
    if (timercmp(&new,timeset,>)) *timeset = new;
}

// check if current time is greater than timeval
// a timeval of zero is treated as infinity !, returning 0
static int check_expired(struct timeval *tv)
{
	struct timeval now;
	
	if (timerisset(tv))
	{
		if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
		if (timercmp(tv,&now,<)) return 1;
	}
	return 0;
}
				
// discard all characters in the input queue of tty
// update frame error counter
static void rx_flush(void)
{
    if ( tcflush(rcxfd,TCIFLUSH)) error_exit("tcflush");
    if ( ioctl(rcxfd,TIOCGICOUNT,&tty_error_info)) error_exit(ioctl);
}

// discard all characters in the output queue of tty
static void tx_flush(void)
{
    if ( tcflush(rcxfd,TCOFLUSH)) error_exit("tcflush");
}

static void rx_error(void)
{
	lnp_integrity_reset();
	timerclear(&inter_byte_time);
	rx_flush();
	set_time(&tx_allowed_time,lnp_byte_safe);
}

static void tx_error(void)
{
	active = 0;
	timerclear(&inter_byte_time);
	tx_flush();
	set_time(&tx_allowed_time,lnp_wait_coll + random() % 16 );
	confirm_packet(TX_FAILURE);
}

static int send_keepalive(int urgent)
{
	int result;
	
	// if no clients, never send keepalive to save battery
	if ( ! get_num_clients() ) return 0;
	
	result = write(rcxfd,&keepalive_byte,1);
	if (result <0) error_exit("write");
	if (result)
	{
		log(LNPD_LOG_LOGICAL,urgent ? "Urgent keepalive sent" : "Keepalive sent");
		set_time(&keep_alive_time,KEEP_ALIVE_TIMEOUT);
		set_time(&tx_allowed_time,lnp_wait_keepalive);
		return 1;
	}
	return 0;
}

static int check_keepalive_urgent(void)
{
	struct timeval now,diff,urgent;
	
	if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
	
	msecs2timeval(KEEP_ALIVE_URGENT,&diff);
	timeradd(&now,&diff,&urgent);
	if (timercmp(&now,&urgent,>))
	{
		return send_keepalive(1);
	}
	return 0;
}

static int rcx_read(void)
{
	int badframe, bytes_read, total_read = 0;
	unsigned char rcv_buffer[RCV_BUFFER_SIZE];
    unsigned char *rcvd;
    struct serial_icounter_struct new_error_info;
		
	while (1)
	{
		// look what´s there
		bytes_read=read(rcxfd,rcv_buffer,RCV_BUFFER_SIZE);
		total_read += bytes_read;
		if ( ! bytes_read) break;
		if ( bytes_read < 0 ) error_exit("read");
        // log(LNPD_LOG_DEBUG,"read %d from tty",bytes_read);
		
		// check for frame errors
        if ( ioctl(rcxfd,TIOCGICOUNT,&new_error_info)) error_exit("ioctl");
        badframe =
            ( new_error_info.frame != tty_error_info.frame ) ||
            ( new_error_info.overrun != tty_error_info.overrun );
        tty_error_info = new_error_info;
        if (badframe)
        {
        	if (active)
        	{
        		tx_error();
	        	rx_flush();
	        }
       		else rx_error();
         	log(LNPD_LOG_LOGICAL,"Frame Error");
        	break;
        }

		for (rcvd = rcv_buffer; rcvd < rcv_buffer + bytes_read ; ++rcvd)
		{
			if (active)
			{
				if (*rcvd != *tx_verify)
				{
					log(LNPD_LOG_LOGICAL,"Transmit Collision");
					tx_error();
					rx_flush();
					goto done;
				}
				else if (++tx_verify >= tx_end)
				{
					active = 0;
					set_time(&tx_allowed_time,lnp_wait_txok);
					confirm_packet(TX_SUCCESS);
					log(LNPD_LOG_LOGICAL,"Transmitted Packet Len %d",tx_end-tx_buffer);
				}
			}
			else
			{
				lnp_integrity_byte(*rcvd);
				if ( !lnp_integrity_active() || !check_keepalive_urgent() )
					set_time(&tx_allowed_time,lnp_byte_safe);
			}
		}
	}
	done:	
	if ( lnp_integrity_active() || active )
		set_time(&inter_byte_time,lnp_byte_timeout);
	else timerclear(&inter_byte_time);
	return total_read;
}

static int rcx_write(void)
{
	int length, written = 0;
	if ( !active )
	{
		if ( check_expired(&tx_allowed_time) )
		{
			if ( (length = get_packet(tx_buffer)) )
			{
				tx_end = tx_buffer + length;
				tx_next = tx_buffer;
				tx_verify = tx_buffer;
				active = 1;
				set_time(&inter_byte_time,lnp_byte_timeout);
				log(LNPD_LOG_LOGICAL,"Transmit packet of length %d started",length);
			}
			else if ( check_expired (&keep_alive_time) )
			{
				return send_keepalive(0);
			}
		}
		else if (check_keepalive_urgent()) return 1;
	}
	if ( active && tx_next < tx_end )
	{
		written = write(rcxfd,tx_next,tx_end-tx_next);
		if (written <0) error_exit("write");
		if (written)
		{
			tx_next+=written;
			set_time(&keep_alive_time,KEEP_ALIVE_TIMEOUT);
		}
	}
	return written;
}

static void init_tower(int highspeed)
{
	unsigned char read_back;

    // wake tower
    write(rcxfd,&keepalive_byte,1);
    set_time(&keep_alive_time,KEEP_ALIVE_TIMEOUT);

    // wait for IR to settle
    msleep(100);
    rx_flush();
    if ( ioctl(rcxfd,TIOCGICOUNT,&tty_error_info)) error_exit("ioctl");

    // check if we can read back what we sent
    write(rcxfd,&keepalive_byte,1);
    msleep(100);

    if ( read(rcxfd,&read_back,1) != 1 )
    {
    	log(LNPD_LOG_FATAL,"no response from tower");
    	exit(1);
    }
}

static void check_interbyte_timeout(void)
{
	if (check_expired(&inter_byte_time))
	{
		log(LNPD_LOG_LOGICAL,"Inter-Byte Timeout");
		if (active) tx_error();
		else rx_error();
	}
}

void init_transceiver(int highspeed, unsigned extra_wait, int rcx_fd)
{
	struct timeval now;
	
	rcxfd = rcx_fd;
	
	// initialise random generator
	if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
	srandom(now.tv_usec);
	
	// prepare some variables
	lnp_set_speed(highspeed,extra_wait);
	set_time(&tx_allowed_time,0);
	
	// prepare tower
	init_tower(highspeed);
}

void run_transceiver(void)
{
	fd_set readset,writeset;
	int result,maxno;
	struct timeval timeout;
	
	// loop forever
	while (1)
	{
		FD_ZERO(&readset);
		FD_SET(rcxfd,&readset);
		FD_ZERO(&writeset);
		if (active && ( tx_next < tx_end)) FD_SET(rcxfd,&writeset);
		msecs2timeval(LOOP_TIMEOUT,&timeout);
		maxno = MAX(rcxfd,set_socket_descriptors(&readset,&writeset));

		result = select(maxno+1,&readset,&writeset,NULL, &timeout);
		
		if ( result < 0 && errno != EINTR ) error_exit("select()");
		
		if ( FD_ISSET(rcxfd,&readset)) result=rcx_read();
		else check_interbyte_timeout();
		if ( !lnp_integrity_active() ) rcx_write();
		
		serv_clients(&readset,&writeset);
	}
}

