//
// liblnp implementation
//

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <memory.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>

//for debugging
#include <stdio.h>

#include "liblnp.h"


#define DEFAULT_PORT 7776
#define DEFAULT_LNP_ADDR 0x80
#define DEFAULT_LNP_MASK 0xf0
//#define SIG_RCV SIGRTMAX
#define SIG_RCV SIGIO
#define MAX_LNP_PACKET (256+3)
#define TRANSMIT_TIMEOUT 5000
#define ACK_TIMEOUT 1000
#define LNPD_ACK_OK 0x33
#define LNPD_ACK_BAD 0x77

typedef enum { RCV_WAIT_FIRST, RCV_WAIT_LEN, RCV_WAIT_DATA } rcv_state_t;

static unsigned char lnp_host_address;
static unsigned char lnp_host_mask;
static unsigned char lnp_port_mask;
static int discard_while_tx;
static int socket_fd;
static int connected;
static int tx_active;
static lnp_tx_result tx_result;
static unsigned char lnp_buffer[MAX_LNP_PACKET];
static lnp_integrity_handler_t integrity_handler;
static lnp_addressing_handler_t addressing_handler[256];
static rcv_state_t rcv_state;

// convert a time in milliseconds to struct timeval
static void msecs2timeval(int msecs, struct timeval *tval)
{
	tval->tv_sec = msecs / 1000;
	tval->tv_usec = msecs*1000 % 1000000;
}

static unsigned short lnp_checksum( const unsigned char *data, unsigned length )
{
  unsigned char a = 0xff;
  unsigned char b = 0xff;

  while (length > 0) {
    a = a + *data;
    b = b + a;
    data++;
    length--;
  }

  return a + (b << 8);
}

void block_rcv(void)
{
	sigset_t newset;
	sigemptyset(&newset);
	sigaddset(&newset,SIG_RCV);
	sigprocmask(SIG_BLOCK,&newset,NULL);
}

void unblock_rcv(void)
{
	sigset_t newset;
	sigemptyset(&newset);
	sigaddset(&newset,SIG_RCV);
	sigprocmask(SIG_UNBLOCK,&newset,NULL);
}

void lnp_integrity_set_handler(lnp_integrity_handler_t handler)
{
	block_rcv();
	integrity_handler = handler;
	unblock_rcv();
}

void lnp_addressing_set_handler(unsigned char port, lnp_addressing_handler_t handler)
{
	block_rcv();
	addressing_handler[port] = handler;
	unblock_rcv();
}

void lnp_shutdown(void)
{
    struct sigaction saction;
    	
    block_rcv();
	if (connected) close(socket_fd);
	saction.sa_handler = SIG_IGN;
    sigemptyset(&saction.sa_mask);
    saction.sa_flags = 0;
    sigaction(SIG_RCV,&saction,NULL);
	connected = 0;
	tx_active = 0;
	rcv_state = RCV_WAIT_FIRST;
	unblock_rcv();
}

static void lnp_receive_packet(const unsigned char *data) {
    unsigned char header=*(data++);
    unsigned char length=*(data++);

    // only handle non-degenerate packets in boot protocol 0xf0
    //
    switch(header) {
    case 0xf0:	      // raw integrity layer packet, no addressing.
        if(integrity_handler)
            integrity_handler(data,length);
        break;

    case 0xf1:	      // addressing layer.
        if(length>2) {
            unsigned char dest=*(data++);

            if( lnp_host_address == (dest & lnp_host_mask)) {
                unsigned char port=dest & lnp_port_mask;
	
                if(addressing_handler[port]) {
                    unsigned char src=*(data++);
                    addressing_handler[port](data,length-2,src);
                }
            }
        }
    } // switch(header)
}
	
static int transmit_ack(void)
{
	fd_set fds;
	struct timeval timeout;
	int result;
	static unsigned char ack_byte = LNPD_ACK_OK;
	
	// wait for socket to become writeable
    while (1)
    {
     	FD_ZERO(&fds);
        FD_SET(socket_fd,&fds);
        msecs2timeval( ACK_TIMEOUT, &timeout);
        result = select(socket_fd+1,NULL,&fds,NULL,&timeout);
        if (result == 1 ) break;
        if ( result < 0 && errno == EINTR ) continue;
        // timed out
        return -1;
	}
    // write ACK
    if ( (result = write(socket_fd,&ack_byte,1) ) != 1 )
    	return -1;

    return 0;
}

static int receive_byte(unsigned char byte_read) {
	static unsigned char buffer[MAX_LNP_PACKET];
	static int bytesRead,endOfData;

	if(rcv_state==RCV_WAIT_FIRST)
    	bytesRead=0;

	buffer[bytesRead++]=byte_read;

  	switch(rcv_state)
  	{
    case RCV_WAIT_FIRST:
    	switch ( byte_read )
    	{
    		case LNPD_ACK_OK:
    			tx_result = TX_SUCCESS;
    			tx_active = 0;
    			break;
    		case LNPD_ACK_BAD:
    			tx_result = TX_FAILURE;
    			tx_active = 0;
    			break;
    		default:
    			rcv_state++;
    	}
    	break;
	case RCV_WAIT_LEN:
		endOfData=byte_read+3;
		rcv_state++;
		break;
    case RCV_WAIT_DATA:
		if (bytesRead == endOfData)
		{
      		rcv_state = RCV_WAIT_FIRST;
			if ( transmit_ack() ) return -1;
        	if ( ! (tx_active && discard_while_tx) )
        		lnp_receive_packet(buffer);
      	}
	}
	return 0;
}

static void rcv_handler(int signo)
{
    unsigned char buffer[MAX_LNP_PACKET];
    unsigned char *recvd_byte;
    int length,i;

    // read to buffer
    length = read(socket_fd, buffer, sizeof(buffer));
    if (length < 1 )
    {
    	if (tx_active)
    	{
    		tx_active = 0;
    		tx_result = TX_ERROR;
     	}
    	else lnp_shutdown();
    	return;
    }

	for (i=0,recvd_byte = buffer;i<length;i++,recvd_byte++)
	{
	    if (receive_byte(*recvd_byte))
	    {
	    	if (tx_active)
    		{
    			tx_active = 0;
    			tx_result = TX_ERROR;
	     	}
    		else lnp_shutdown();
	    	return;
	    }
	}   	
}

static lnp_tx_result lnp_logical_write (unsigned char *data, int length)
{
	int written=0, result;
	sigset_t newset,oldset;
    fd_set fds;
    struct timeval timeout;

    if (!connected) return TX_ERROR;
	
	// block rcv signal, saving old mask
	sigemptyset(&newset);
	sigaddset(&newset,SIG_RCV);
	sigprocmask(SIG_BLOCK,&newset,&oldset);
	
	tx_active = 1;
	
	// send the packet
    while ( written != length )
    {
        // wait for socket to become writeable
        while (1)
        {
            FD_ZERO(&fds);
            FD_SET(socket_fd,&fds);
            msecs2timeval( TRANSMIT_TIMEOUT, &timeout);
            result = select(socket_fd+1,NULL,&fds,NULL,&timeout);
            if (result == 1 ) break;
            if ( result < 0 && errno == EINTR ) continue;
            // timed out
           	lnp_shutdown();
           	return TX_ERROR;
        }
        // write as much as possible
        result =  write(socket_fd,data+written,length-written);
        if ( result < 1 )
        {
           	lnp_shutdown();
           	return TX_ERROR;
        }
		written += result;
    }

    // wait for result
    while (1)
    {
        sigsuspend(&oldset);
        if ( !tx_active) break;
    }

    if (tx_result == TX_ERROR) lnp_shutdown();

    unblock_rcv();
	
	return tx_result;
}	

lnp_tx_result lnp_integrity_write(const unsigned char *data,unsigned char length) {
  lnp_buffer[0]=0xf0;
  lnp_buffer[1]=length;
  memcpy(lnp_buffer+2,data,length);
  lnp_buffer[length+2]=(unsigned char) lnp_checksum(lnp_buffer,length+2);

  return lnp_logical_write(lnp_buffer,length+3);
}

lnp_tx_result lnp_addressing_write(const unsigned char *data,unsigned char length,
                         unsigned char dest,unsigned char srcport) {
  lnp_buffer[0]=0xf1;
  lnp_buffer[1]=length+2;
  lnp_buffer[2]=dest;
  lnp_buffer[3]=lnp_host_address | (srcport & lnp_port_mask);
  memcpy(lnp_buffer+4,data,length);
  lnp_buffer[length+4]=(unsigned char) lnp_checksum(lnp_buffer,length+4);

  return lnp_logical_write(lnp_buffer,length+5);
}

lnp_init_result lnp_init ( char *tcp_hostname, unsigned short tcp_port,
	unsigned char lnp_address, unsigned char lnp_mask, int flags )
{
	struct hostent* hostentry;
	struct in_addr hostaddress;
	struct linger opt_nolinger = { 0 , 0 };
	struct sockaddr_in server_address;
    struct sigaction saction;
	
	lnp_shutdown();
	
	// get ip-address of host
	if (!tcp_hostname)
		inet_aton("127.0.0.1",&hostaddress);
	else if ( ! inet_aton(tcp_hostname, &hostaddress) )
	{
		// resolve hostname
		hostentry = gethostbyname(tcp_hostname);
		if ( !hostentry ) return INIT_BAD_PARAM;
		hostaddress = *(struct in_addr*)(hostentry->h_addr);
	}
	
	// check other parameters
	if (!tcp_port) tcp_port = DEFAULT_PORT;
	lnp_host_address = lnp_address ? lnp_address : DEFAULT_LNP_ADDR;
	lnp_host_mask = lnp_mask ? lnp_mask : DEFAULT_LNP_MASK;
	lnp_port_mask = lnp_host_mask ^ 0xFF;
	if ( ( lnp_host_address & lnp_host_mask ) != lnp_host_address ) return INIT_BAD_PARAM;
	discard_while_tx = ( ( flags & LNP_DISCARD_WHILE_TX ) == LNP_DISCARD_WHILE_TX );
	
	// create socket
	if ( ( socket_fd = socket(PF_INET,SOCK_STREAM,0)) < 0 ) return INIT_ERROR;
	setsockopt( socket_fd, SOL_SOCKET, SO_LINGER, &opt_nolinger, sizeof(opt_nolinger) );
 	
    block_rcv();
	
	connected = 1;
	
	// setup signal handler
	saction.sa_handler = rcv_handler;
    sigemptyset(&saction.sa_mask);
    saction.sa_flags = 0;
    sigaction(SIG_RCV,&saction,NULL);

	// connect
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(tcp_port);
	server_address.sin_addr = hostaddress;
	if (connect(socket_fd, (struct sockaddr*)&server_address,sizeof(server_address)))
	{
		lnp_shutdown();
		return INIT_ERROR;
	}

    if
    (
    	fcntl(socket_fd,F_SETFL,O_ASYNC | O_NONBLOCK) ||
    	fcntl(socket_fd,F_SETOWN,getpid()) ||
	    fcntl(socket_fd,F_SETSIG,SIG_RCV)
	)
	{
		lnp_shutdown();
		return INIT_ERROR;
	}
	
	unblock_rcv();
	return INIT_OK;
}

