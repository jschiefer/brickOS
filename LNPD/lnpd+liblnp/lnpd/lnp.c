/*
 *  The Original Code is legOS code, released October 17, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s): Markus L. Noga <markus@noga.de>
 */

#include <string.h>

#include "logger.h"
#include "lnp.h"
#include "daemon.h"

int lnp_byte_timeout;
int lnp_byte_safe;
int lnp_wait_coll;
int lnp_wait_txok;
int lnp_wait_keepalive;

// states for the integrity layer state machine
typedef enum
{
  LNPwaitHeader,
  LNPwaitLength,
  LNPwaitData,
  LNPwaitCRC
} lnp_integrity_state_t;

// the integrity layer state
static lnp_integrity_state_t lnp_integrity_state;

// compute checksum of lnp packet
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

// set speed of lnp
void lnp_set_speed(int highspeed, unsigned extra_wait)
{
	int lnp_byte_time	= highspeed ? LNP_BYTE_FAST : LNP_BYTE_SLOW;
	lnp_byte_timeout	= lnp_byte_time * LNP_BYTE_TIMEOUT_FACTOR;
	lnp_byte_safe		= lnp_byte_time * LNP_BYTE_SAFE_FACTOR + extra_wait;
	lnp_wait_coll		= lnp_byte_timeout * LNP_WAIT_COLL_FACTOR + extra_wait;
	lnp_wait_txok		= lnp_byte_timeout * LNP_WAIT_TXOK_FACTOR + extra_wait;
	lnp_wait_keepalive	= lnp_wait_txok;
}

// receive a byte, decoding LNP packets with a state machine.
void lnp_integrity_byte(unsigned char b)
{
	static unsigned char buffer[MAX_LNP_PACKET];
  	static int bytesRead,endOfData;

  	if(lnp_integrity_state==LNPwaitHeader)
    	bytesRead=0;

  	buffer[bytesRead++]=b;

	switch(lnp_integrity_state)
	{
    case LNPwaitHeader:
    	// valid headers are 0xf0 .. 0xf7
		if((b & (unsigned char) 0xf8) == (unsigned char) 0xf0)
      	{
      		logmsg(LNPD_LOG_INTEGRITY,"Header %2x",(unsigned)b);
      		lnp_integrity_state++;
      	}
      	break;

   	case LNPwaitLength:
       	logmsg(LNPD_LOG_INTEGRITY,"Length %u",(unsigned)b);
	   	endOfData=b+2;
      	lnp_integrity_state++;
      	break;

    case LNPwaitData:
      	if(bytesRead==endOfData)
		lnp_integrity_state++;
	    break;

    case LNPwaitCRC:
      	if(b == (unsigned char)lnp_checksum(buffer,endOfData))
      	{
      		logmsg(LNPD_LOG_INTEGRITY,"Packet received");
	        deliver_packet(buffer,bytesRead);
	    }
	    else logmsg(LNPD_LOG_INTEGRITY,"Bad checksum");
	    lnp_integrity_reset();
	}
}

// reset the integrity layer on error or timeout.
void lnp_integrity_reset(void)
{
    logmsg(LNPD_LOG_INTEGRITY,"Integrity reset");
    lnp_integrity_state=LNPwaitHeader;
}

// return whether a packet is currently being received
int lnp_integrity_active(void)
{
  	return lnp_integrity_state != LNPwaitHeader;
}

