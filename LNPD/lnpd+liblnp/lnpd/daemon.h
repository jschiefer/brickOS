//
// header for daemon.c
//

#ifndef DAEMON_H
#define DAEMON_H

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>

#include "lnp.h"
#include "transceiver.h"

#define LNPD_ACK_OK 0x33
#define LNPD_ACK_BAD 0x77

typedef enum
{
	LNPD_READ_IDLE = 0,
	LNPD_READ_GOT_HEADER,
	LNPD_READ_RECEIVING,
	LNPD_READ_WAIT_TRANCSEIVER,
	LNPD_READ_WAIT_RESULT,
	LNPD_READ_WAIT_ACCESS,
	LNPD_READ_SENDING_ACK,
} lnpd_rcv_state_t;

typedef enum
{
	LNPD_WRITE_IDLE = 0,
	LNPD_WRITE_WAIT_ACCESS,
	LNPD_WRITE_SENDING_PACKET,
	LNPD_WRITE_WAIT_ACK,
} lnpd_tx_state_t;

typedef struct lnpd_client_info_struct
{
	int	descriptor;
	lnpd_rcv_state_t rcv_state;
	lnpd_tx_state_t tx_state;
	unsigned char rcv_buffer[MAX_LNP_PACKET];
	unsigned char tx_buffer[MAX_LNP_PACKET];
	lnp_tx_result tx_result;
	unsigned char *next_to_rcv;
	int	packet_length;
	unsigned char *next_to_tx;
	unsigned char *tx_end;
	struct lnpd_client_info_struct* next;
    struct lnpd_client_info_struct* previous;
    unsigned int identity;
} lnpd_client_info_t;

// number of clients currently connected
extern int get_num_clients(void);

// initialise daemon
extern void daemon_init( unsigned short port, int maxclients );

// transfer packet to buffer and return length if packet available
// else return 0
// Max len = MAX_LNP_PACKET-1 !!
extern int get_packet(unsigned char* buffer);

// notify confirmation of last delivered packet
extern void confirm_packet(lnp_tx_result status);

// packet has arrived, deliver to all clients
extern void deliver_packet(unsigned char *packet, int len);

// set socket descriptors that must be watched
// returns: highest descriptor number
extern int set_socket_descriptors(fd_set *readset,fd_set *writeset);

// work on sockets, nonblockingly !!
extern void serv_clients(fd_set *readset,fd_set *writeset);		

#endif