//
// Header for transceiver.c
//

#ifndef TRANSCEIVER_H
#define TRANSCEIVER_H

// out come of a packet transmit
typedef enum { TX_SUCCESS, TX_FAILURE } lnp_tx_result;

// initialise transceiver
extern void init_transceiver(int highspeed, unsigned extra_wait, int rcx_fd);

// run transceievr, never comes back
extern void run_transceiver(void);

#endif