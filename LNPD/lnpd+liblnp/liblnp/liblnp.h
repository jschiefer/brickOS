//
// API for liblnp
//

#ifndef LIBLNP_H
#define LIBLNP_H

// the integrity layer packet handler type
// arguments are (data,length).
typedef void (*lnp_integrity_handler_t) (const unsigned char *, unsigned char);

// the addressing layer packet handler type
// arguments are (data,length,src_address).
typedef void (*lnp_addressing_handler_t) (const unsigned char *, unsigned char, unsigned char);

// result of init
typedef enum { INIT_OK, INIT_BAD_PARAM, INIT_ERROR } lnp_init_result;

// out come of a packet transmit
typedef enum { TX_SUCCESS, TX_FAILURE, TX_ERROR } lnp_tx_result;

#define LNP_DISCARD_WHILE_TX 1

// initialise, connect to lnpd
// parameters:
// tcp_hostname:
//	host running lnpd to connect - ascii or dotted quad
//	0 means: default to localhost
// tcp_port:
//  portnumber to connect to
//  0 means: default to 7776
// lnp_address:
//  LNP hostaddress to use
//  0 means: default to 0x80
// lnp_mask:
//  LNP hostmask to use
//  0 means: default to 0xF0
// flags:
//	bitwise or of:
//  LNP_DISCARD_WHILE_TX :
//		discard all packets received while waiting for tx result
//
// returns:
//	INIT_OK 		ok, connected to lnp-daemon
//	INIT_BAD_PARAM 	bad parameters
//	INIT_ERROR      network error, errno is set
//	
extern lnp_init_result lnp_init
(
	char *tcp_hostname,
	unsigned short tcp_port,
	unsigned char lnp_address,
	unsigned char lnp_mask,
	int flags
);

// shutdown lnp, disconnect
extern void lnp_shutdown(void);

// set the integrity layer packet handler
extern void lnp_integrity_set_handler(lnp_integrity_handler_t handler);

// set an addressing layer packet handler for a lnp port.
extern void lnp_addressing_set_handler(unsigned char port, lnp_addressing_handler_t handler);

// send a LNP integrity layer packet of given length
// returns:
//	TX_SUCCESS success
//	TX_FAILURE collision
//	TX_ERROR   network error, errno is set
//
extern lnp_tx_result lnp_integrity_write(const unsigned char *data,unsigned char length);

// send a LNP addressing layer packet of given length
// returns:
//	TX_SUCCESS success
//	TX_FAILURE collision
//	TX_ERROR   network error, errno is set
//
extern lnp_tx_result lnp_addressing_write(const unsigned char *data,unsigned char length,
                         unsigned char dest,unsigned char srcport);

//
// prevent all handlers from being invoked
//
extern void block_rcv(void);

//
// unblock handlers
//
extern void unblock_rcv(void);

#endif






