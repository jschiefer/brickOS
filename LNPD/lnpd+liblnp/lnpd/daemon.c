//
// the daemon
//

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#include "daemon.h"
#include "logger.h"

#define MAX(a,b) ( (a) > (b) ? (a) : (b) )

static int server_socket;
static int num_clients;
static int max_clients;

// linked list of clients
static lnpd_client_info_t* client_info_list;
static lnpd_client_info_t* next_client_to_transmit;
static lnpd_client_info_t* client_to_confirm;
static int server_running;
static unsigned short myport;
static unsigned int connect_count;

int get_num_clients(void)
{
	return num_clients;
}

void start_server(void)
{	
	struct sockaddr_in address;
	struct linger opt_nolinger = { 0 , 0 };
	int opt_true = 1;
	
	// create the server socket	
	server_socket = socket( PF_INET, SOCK_STREAM, 0);
	if ( server_socket < 0 ) error_exit("socket");
	
	// let socket reuse address
	if ( setsockopt( server_socket, SOL_SOCKET, SO_REUSEADDR, &opt_true, sizeof(opt_true) ) )
		error_exit("setsockopt"); 	 	 	 	
	// don´t linger on close
 	if ( setsockopt( server_socket, SOL_SOCKET, SO_LINGER, &opt_nolinger, sizeof(opt_nolinger) ))
 		error_exit("setsockopt");
 	
 	address.sin_family = AF_INET;
 	address.sin_port = htons(myport);
 	address.sin_addr.s_addr = INADDR_ANY;
 	
 	// set address and port
 	if ( bind(server_socket, (struct sockaddr *)&address, sizeof(address) ) )
 		error_exit("bind");
 	
 	// make sure accept will never block,
 	// implies all accepted connections also will be nonblocking
 	if ( fcntl ( server_socket, F_SETFL, O_NONBLOCK ) )
 		error_exit("fcntl");
 		
 	// make socket listening
 	if ( listen( server_socket, 3 ) )
 		error_exit("listen");
 		
 	server_running = 1;
}

void daemon_init( unsigned short port, int maxclients )
{
	// ignore SIGPIPE, writing to broken socket will return EPIPE instead
	signal(SIGPIPE,SIG_IGN);
	
	myport = port;
	max_clients = maxclients;
	start_server();
}
	
void stop_server(void)
{
	if (close(server_socket)) error_exit("close");
	server_running = 0;
}

int set_socket_descriptors(fd_set *readset,fd_set *writeset)
{
	lnpd_client_info_t* client_info;
	int max_fd = 0;

	// check server socket only if not too much clients
	if (server_running)
	{
		FD_SET(server_socket,readset);
		max_fd = MAX( max_fd, server_socket );
	}
	
	// run through client sockets
	for ( client_info = client_info_list; client_info; client_info = client_info->next )
	{
        // check if we must read from client
		if
		(
			client_info->tx_state == LNPD_WRITE_WAIT_ACK ||
			client_info->rcv_state == LNPD_READ_IDLE ||
			client_info->rcv_state == LNPD_READ_GOT_HEADER ||
			client_info->rcv_state == LNPD_READ_RECEIVING
		)
		{
			FD_SET( client_info->descriptor, readset );
			max_fd = MAX( max_fd, client_info->descriptor );
		}
		// check if we must write to client
		if
		(
			client_info->tx_state == LNPD_WRITE_SENDING_PACKET ||
			client_info->rcv_state == LNPD_READ_SENDING_ACK
		)
		{
			FD_SET( client_info->descriptor, writeset );
			max_fd = MAX( max_fd, client_info->descriptor );
		}
	}
	return max_fd;
}
			
int get_packet(unsigned char * buffer)
{
	lnpd_client_info_t* current_client = next_client_to_transmit;

	// if there is no client left, return immediately	
	if (!current_client) return 0;
	
	// trying to be fair to our clients
	// we ask them round robin for packets to transmit
	while (1)
	{
		if (current_client->rcv_state == LNPD_READ_WAIT_TRANCSEIVER)
		{
			logmsg(LNPD_LOG_CLIENT,"transmitting packet from client %u",current_client->identity);
			// copy packet to transceiver
			memcpy(buffer,current_client->rcv_buffer,current_client->packet_length);
			current_client->rcv_state = LNPD_READ_WAIT_RESULT;
			client_to_confirm = current_client;
			// advance to next client
			next_client_to_transmit =
				current_client->next ? current_client->next : client_info_list;
			return current_client->packet_length;
		}
		// check next one
		current_client = current_client->next ? current_client->next : client_info_list;
		// if run through the whole list, break out
		if (current_client == next_client_to_transmit) break;
	}
	
	return 0;
}

extern void confirm_packet(lnp_tx_result status)
{
	// client might have died meanwhile...
	if ( client_to_confirm )
	{
		client_to_confirm->tx_result = status;
		client_to_confirm->rcv_state = LNPD_READ_WAIT_ACCESS;
	}
}

extern void deliver_packet(unsigned char *packet, int length)
{
	lnpd_client_info_t* cinfo;
	
	// deliver packet to all clients
	for ( cinfo = client_info_list; cinfo; cinfo = cinfo->next )
	{
 		logmsg(LNPD_LOG_CLIENT,"delivered to %u, state %d",cinfo->identity,cinfo->tx_state);
 		
		// if client tx machine is not IDLE, packet is not sent
		// hopefully this will never happen
		if ( cinfo->tx_state == LNPD_WRITE_IDLE )
		{
			memcpy(cinfo->tx_buffer,packet,length);
			cinfo->next_to_tx = cinfo->tx_buffer;
			cinfo->tx_end = cinfo->tx_buffer + length;
			cinfo->tx_state = LNPD_WRITE_WAIT_ACCESS;
		}
	}
}

static void append_to_list(lnpd_client_info_t *client_info)
{
	lnpd_client_info_t *oldhead = client_info_list;
	
	client_info_list = client_info;
	client_info->next = oldhead;
	if (oldhead)
		oldhead->previous = client_info;
		
	if (!next_client_to_transmit) next_client_to_transmit = client_info;
		
 	++num_clients;
}

static void check_new_clients(fd_set *fileset)
{
	int client_fd;
	struct sockaddr_in client_address;
	socklen_t addrlength = sizeof(struct sockaddr_in);
	lnpd_client_info_t *client_info;
	struct linger opt_nolinger = { 0 , 0 };
	
	if ( server_running && FD_ISSET ( server_socket, fileset ) )
	{
		client_fd = accept( server_socket, (struct sockaddr *)&client_address, &addrlength );
		if ( client_fd < 0 )
		{
			// accept will return errors in case of network problems
			// so we don´t exit here but log the error
			logmsg(LNPD_LOG_INFO,"accept() error: %s",strerror(errno));
			return;
		}
		
		client_info = malloc(sizeof( lnpd_client_info_t ));
		if ( !client_info ) error_exit("malloc");
		
		// don´t linger on close
 		if ( setsockopt( client_fd, SOL_SOCKET, SO_LINGER, &opt_nolinger, sizeof(opt_nolinger) ))
 			error_exit("setsockopt");
 			
 		memset(client_info,0,sizeof( lnpd_client_info_t ));
 		client_info->descriptor = client_fd;
 		client_info->identity = connect_count++;
 		
 		append_to_list(client_info);
 		
 		// we cannot look up the hostname, might take far too long 8-(
 		logmsg(LNPD_LOG_CLIENT,"connection %u from host %s, port %hu",
 			client_info->identity,
 			inet_ntoa(client_address.sin_addr),
 			ntohs(client_address.sin_port));
 		
 		if (num_clients >= max_clients) stop_server();
 	}
}

static void delete_client(lnpd_client_info_t* cinfo)
{
 	logmsg(LNPD_LOG_CLIENT,"client %u closed",cinfo->identity),
 	
 	--num_clients;
 	
	if ( close(cinfo->descriptor) )
		error_exit("close");
	
	if (client_to_confirm == cinfo)
		client_to_confirm = NULL;
		
	if (cinfo->previous)
		cinfo->previous->next = cinfo->next;
	else
		// was first in linked list
		client_info_list = cinfo->next;
		
	if (cinfo->next)
		cinfo->next->previous = cinfo->previous;
		
	if ( next_client_to_transmit == cinfo )
		 next_client_to_transmit = cinfo->next ? cinfo->next : client_info_list;
		
	free(cinfo);
	if (num_clients < max_clients && !server_running) start_server();
}
		
// process receiving machine
// returns 0 if ok, -1 on error
static int process_rcv( lnpd_client_info_t* cinfo,fd_set *readset,fd_set *writeset)
{
	int fd = cinfo->descriptor;
	int readable = FD_ISSET( fd , readset );
	int writeable = FD_ISSET( fd , writeset );
	int result;
	
	switch ( cinfo->rcv_state )
	{
	case LNPD_READ_IDLE:
		if ( readable )
		{
			cinfo->next_to_rcv = cinfo->rcv_buffer;
			if ( (result = read( fd , cinfo->next_to_rcv, 1) ) < 1 )
				// socket closed or error
				return -1;
			
			logmsg(LNPD_LOG_CLIENT,"READ_IDLE read %2X from client %u",*cinfo->next_to_rcv,cinfo->identity);
			
			// is it an ACK we´re waiting for ?
			if ( *cinfo->next_to_rcv == LNPD_ACK_OK && cinfo->tx_state == LNPD_WRITE_WAIT_ACK )
			{
				cinfo->tx_state = LNPD_WRITE_IDLE;
				break;
			}					
			// is it a valid LNP header ?
			else if((*cinfo->next_to_rcv & (unsigned char) 0xf8) == (unsigned char) 0xf0)
			{
				cinfo->rcv_state = LNPD_READ_GOT_HEADER;
				cinfo->next_to_rcv++;
				break;
			}
			// looks like client is buggy, close socket
			return -1;
		}
		// select said there´s nothing to read, o.k.
		break;
	case LNPD_READ_GOT_HEADER:
		if ( readable )
		{
			if ( (result = read( fd , cinfo->next_to_rcv, 1) ) < 1 )
				// socket closed or error
				return -1;
			
			logmsg(LNPD_LOG_CLIENT,"READ_GOT_HEADER read length %u from client %u",(unsigned)*cinfo->next_to_rcv,cinfo->identity);
			
			cinfo->packet_length = *cinfo->next_to_rcv + 3;
			cinfo->next_to_rcv++;
			cinfo->rcv_state = LNPD_READ_RECEIVING;
		}
		// select said there´s nothing to read, o.k.
		break;
	case LNPD_READ_RECEIVING:
		if ( readable )
		{
			int bytes_left = cinfo->packet_length - ( cinfo->next_to_rcv - cinfo->rcv_buffer);
			
			if ( (result = read( fd , cinfo->next_to_rcv, bytes_left) ) < 1 )
				// socket closed or error
				return -1;
			else
			{
				logmsg(LNPD_LOG_CLIENT,"READ_Receiving read %d bytes client %u",result,cinfo->identity);
				if (result == bytes_left)
					// complete packet received,
					cinfo->rcv_state = LNPD_READ_WAIT_TRANCSEIVER;
				else
					cinfo->next_to_rcv+=result;
			}
		}
		break;
	case LNPD_READ_WAIT_TRANCSEIVER:
		// nothing to do, this state is left via get_packet()
		break;
	case LNPD_READ_WAIT_RESULT:
		// nothing to do, this state is left via confirm_packet()
		break;
	case LNPD_READ_WAIT_ACCESS:
		// check if transceiver machine is currently sending
		if (cinfo->tx_state != LNPD_WRITE_SENDING_PACKET)
			cinfo->rcv_state = LNPD_READ_SENDING_ACK;
		break;
	case LNPD_READ_SENDING_ACK:
		if ( writeable )
		{
			unsigned char ack = ( cinfo->tx_result == TX_SUCCESS ) ?  LNPD_ACK_OK : LNPD_ACK_BAD;
			if ( write( fd, &ack, 1 ) != 1 )
				return -1;
			cinfo->rcv_state = LNPD_READ_IDLE;
		}
	}
	return 0;
}

// process transmitting machine
// returns 0 if ok, -1 on error
static int process_tx( lnpd_client_info_t* cinfo,fd_set *readset,fd_set *writeset)
{
	int fd = cinfo->descriptor;
	int readable = FD_ISSET( fd , readset );
	int writeable = FD_ISSET( fd , writeset );
	int result;
	
	switch( cinfo->tx_state)
	{
	case LNPD_WRITE_IDLE:
		// nothing to do, this state is left via deliver_packet()
		break;
	case LNPD_WRITE_WAIT_ACCESS:
		// check if receiver machine is currently sending
		if (cinfo->rcv_state != LNPD_READ_SENDING_ACK)
			cinfo->tx_state = LNPD_WRITE_SENDING_PACKET;
		break;
	case LNPD_WRITE_SENDING_PACKET:
		if (writeable)
		{
			result = write( cinfo->descriptor, cinfo->next_to_tx, cinfo->tx_end - cinfo->next_to_tx);
			if (result < 1)
				// socket closed or error;
				return -1;
				
			logmsg(LNPD_LOG_CLIENT,"wrote %d bytes to client %u",result,cinfo->identity),

			cinfo->next_to_tx += result;
			if (cinfo->next_to_tx >= cinfo->tx_end)
				cinfo->tx_state = LNPD_WRITE_WAIT_ACK;
		}
		break;
	case LNPD_WRITE_WAIT_ACK:
		if (readable)
		{
			result = read(cinfo->descriptor, &cinfo->tx_result, 1);
			if (result != 1) return -1;
			if (cinfo->tx_result != LNPD_ACK_OK && cinfo->tx_result != LNPD_ACK_BAD )
				// buggy client, close it
				return -1;
			cinfo->tx_state = LNPD_WRITE_IDLE;
		}
	}
	return 0;
}

// the really hard work...
void serv_clients(fd_set *readset,fd_set *writeset)
{
	lnpd_client_info_t* cinfo;
	
	// look for new clients first
	check_new_clients(readset);
	
	// walk through list of all clients and process state machines		
	for ( cinfo = client_info_list; cinfo; cinfo = cinfo->next )
	{
		if ( process_rcv(cinfo,readset,writeset) || process_tx(cinfo,readset,writeset) )
			delete_client(cinfo);
	}
}
