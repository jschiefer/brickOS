/*
 *  The Original Code is legOS code, released October 17, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s): Markus L. Noga <markus@noga.de>
 */

#ifndef LNP_H
#define LNP_H

// this are the original LNP constants
#define LNP_BAUD_FAST 			B4800	// baudrate
#define LNP_BYTE_FAST		  	3  		// msecs to transmit a byte
#define LNP_BAUD_SLOW 			B2400
#define LNP_BYTE_SLOW 	    	5
#define LNP_BYTE_TIMEOUT_FACTOR 3  		// * LNP_BYTE_TIME
#define LNP_BYTE_SAFE_FACTOR    4 		// * LNP_BYTE_TIME
#define LNP_WAIT_TXOK_FACTOR	2		// * LNP_BYTE_TIMEOUT
#define LNP_WAIT_COLL_FACTOR   	4		// * LNP_BYTE_TIMEOUT

#define MAX_LNP_PACKET (256+3)

extern int lnp_byte_timeout;
extern int lnp_byte_safe;
extern int lnp_wait_coll;
extern int lnp_wait_txok;
extern int lnp_wait_keepalive;

// set speed of lnp
extern void lnp_set_speed(int highspeed,unsigned extra_wait);

// send byte to integrity layer
extern void lnp_integrity_byte(unsigned char b);

// reset the integrity layer on error or timeout.
extern void lnp_integrity_reset(void);

// return whether a packet is currently being received
// returns 1 if yes, else zero
extern int lnp_integrity_active(void);
		
#endif	// LNP_H

