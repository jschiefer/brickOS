/*! \file   loader.c
    \brief  brickOS task downloading
    \author Markus L. Noga <markus@noga.de>
*/

/*
 *  The contents of this file are subject to the Mozilla Public License
 *  Version 1.0 (the "License"); you may not use this file except in
 *  compliance with the License. You may obtain a copy of the License at
 *  http://www.mozilla.org/MPL/
 *
 *  Software distributed under the License is distributed on an "AS IS"
 *  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 *  License for the specific language governing rights and limitations
 *  under the License.
 *
 *  The Original Code is legOS code, released October 2, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s): everyone discussing LNP at LUGNET
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <string.h>

#include "lx.h"
#include "liblnp.h"

#define MAX_DATA_CHUNK 0xf8   	  //!< maximum data bytes/packet for boot protocol
#define XMIT_RETRIES   8      	  //!< number of packet transmit retries
#define REPLY_TIMEOUT  750000 	  //!< timeout for reply

#define LNP_BYTE_FAST		  	3  		// msecs to transmit a byte
#define LNP_BYTE_SLOW 	    	5

#define DEFAULT_DEST  	0
#define DEFAULT_PROGRAM	0
#define DEFAULT_SRCPORT 0
#define DEFAULT_PRIORITY 10

#define TOWER_RELAX_TIME 100
#define RCX_RELAX_TIME 50
#define ACK_TIMEOUT 200
#define LOOP_WAIT_TIME 20
#define MAX_TX_ERRORS 7
#define MAX_RCV_ERRORS 5

typedef enum {
  CMDacknowledge,     	//!< 1:
  CMDdelete, 	      	//!< 1+ 1: b[nr]
  CMDcreate, 	      	//!< 1+12: b[nr] s[textsize] s[datasize] s[bsssize] s[stacksize] s[start] b[prio]
  CMDoffsets, 	      	//!< 1+ 7: b[nr] s[text] s[data] s[bss]
  CMDdata,   	      	//!< 1+>3: b[nr] s[offset] array[data]
  CMDrun,     	      	//!< 1+ 1: b[nr]
  CMDlast     	      	//!< ?
} packet_cmd_t;

        
static const struct option long_options[]={
  {"rcxaddr",required_argument,0,'r'},
  {"program",required_argument,0,'p'},
  {"srcport",required_argument,0,'s'},
  {"verbose",no_argument      ,0,'v'},
  {"lnp_server",required_argument,0,'l'},
  {0        ,0                ,0,0  }
};

volatile int receivedAck=0;

volatile unsigned short relocate_to=0;

// options defaults
unsigned rcxaddr = DEFAULT_DEST;
unsigned prog = DEFAULT_PROGRAM + 1;	// allow validation to work
unsigned srcport = DEFAULT_SRCPORT;
char* server;
int verbose_flag=0;

#define error_exit(message) { perror(message); exit(1); }

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
		if (!timercmp(&now,&end,<)) break;
		timersub(&end,&now,&wtime);
	} while (1);
}	

// send a LNP addressing packet of given length and wait for ACK
// return 0 on success, -1 on error
int lnp_assured_write(const unsigned char *data,
	unsigned char length,unsigned char dest, unsigned char srcport)
{
	int tx_errors=0,rcv_errors=0;
	lnp_tx_result result;
	struct timeval now,diff,end;
	
	while (1)
	{
		receivedAck = 0;
		// try to send
		result = lnp_addressing_write(data,length,dest,srcport);
		if ( result == TX_ERROR ) break;
		if ( result == TX_FAILURE )
		{
			// tower screwed up ?
			if ( verbose_flag ) fprintf(stderr,"tx_failure %d\n",tx_errors);
			if ( ++tx_errors > MAX_TX_ERRORS) break;
			// give ´em a brake
			msleep(TOWER_RELAX_TIME*(1<<tx_errors));
		}
		else
		{
			// wait for ack or timeout
			if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
			msecs2timeval(ACK_TIMEOUT,&diff);
			timeradd(&now,&diff,&end);
			while (1)
			{
				// we wait only for a short time, because ACK might already be there
				msecs2timeval(LOOP_WAIT_TIME,&diff);
				result = select(0,NULL,NULL,NULL,&diff);
				if ( result < 0 && errno != EINTR ) error_exit("select()");
				if ( receivedAck)
				{
					fprintf(stderr,"transmitted %u\n",(unsigned)length);
					return 0;
				}
				if (gettimeofday(&now,NULL)) error_exit("gettimeofday");
			    if (! timercmp(&now,&end,<)) break;
			}
			// timed out
			if ( verbose_flag ) fprintf(stderr,"rx_timeout %d\n",rcv_errors);
			if ( ++rcv_errors > MAX_RCV_ERRORS) break;
			// give ´em a brake
			msleep(RCX_RELAX_TIME*(1<<rcv_errors));
		}
	}
	return -1;
}
			
void ahandler(const unsigned char *data,unsigned char len,unsigned char src) {
  if(*data==CMDacknowledge) {
    receivedAck=1;
    if(len==8) {
      // offset packet
      //
      relocate_to=(data[2]<<8)|data[3];
    }
  }
}

    
void lnp_download(const lx_t *lx) {
  unsigned char buffer[256+3];
  
  size_t i,chunkSize,totalSize=lx->text_size+lx->data_size;

  if(verbose_flag)
    fputs("data\n",stderr);

  buffer[0]=CMDdata;
  buffer[1]=prog;

  for(i=0; i<totalSize; i+=chunkSize) {
    chunkSize=totalSize-i;
    if(chunkSize>MAX_DATA_CHUNK)
      chunkSize=MAX_DATA_CHUNK;

    buffer[2]= i >> 8;
    buffer[3]= i &  0xff;
    memcpy(buffer+4,lx->text + i,chunkSize);
    if(lnp_assured_write(buffer,chunkSize+4,rcxaddr,srcport)) {
      fputs("error downloading program\n",stderr);
      exit(-1);
    }
  }
}

int main(int argc, char **argv) {
  lx_t lx;    	    // the brickOS executable
  char *filename;
  int opt,option_index;
    
  unsigned char buffer[256+3];

  while((opt=getopt_long(argc, argv, "r:p:s:l:v",
                        long_options, &option_index) )!=-1) {
    switch(opt) {
      case 'r':
		sscanf(optarg,"%x",&rcxaddr);
        break;
      case 'p':
		sscanf(optarg,"%x",&prog);
        break;
      case 's':
		sscanf(optarg,"%x",&srcport);
        break;
      case 'v':
		verbose_flag=1;
		break;
	  case 'l':
	  	server = strdup(optarg);
		break;
    }
  }           
  if(prog < 1 || prog > 8) {
    fprintf(stderr,"Error: -p%d invalid (BrickOS supports [1-8]).\n",prog);
    return -1;
  } else {
	prog--;	// make zero relative
  }
  
  // load executable
  //      
  if(argc-optind<1) {
    fprintf(stderr,"usage: %s file.lx\n"
	           "[-rrcxaddress -pprogramnumber -ssrcport -lserver -v]\n",
	           argv[0]);
    return -1;
  }
  filename=argv[optind++];
  if(lx_read(&lx,filename)) {
    fprintf(stderr,"unable to load brickOS executable from %s.\n",filename);
    return -1;
  }
      
  if (lnp_init(server,0,0,0,0))
  {
  	fprintf(stderr,"unable to connect Tower\n");
  	return -1;
  }

  lnp_addressing_set_handler(0,ahandler);


  if(verbose_flag)
    fputs("delete\n",stderr);
  buffer[0]=CMDdelete;
  buffer[1]=prog; //       prog 0
  if(lnp_assured_write(buffer,2,rcxaddr,srcport)) {
    fputs("error deleting program\n",stderr);
    return -1;
  }

  if(verbose_flag)
    fputs("create\n",stderr);
  buffer[ 0]=CMDcreate;
  buffer[ 1]=prog; //       prog 0
  buffer[ 2]=lx.text_size>>8;
  buffer[ 3]=lx.text_size & 0xff;
  buffer[ 4]=lx.data_size>>8;
  buffer[ 5]=lx.data_size & 0xff;
  buffer[ 6]=lx.bss_size>>8;
  buffer[ 7]=lx.bss_size & 0xff;
  buffer[ 8]=lx.stack_size>>8;
  buffer[ 9]=lx.stack_size & 0xff;
  buffer[10]=lx.offset >> 8;  	// start offset from text segment
  buffer[11]=lx.offset & 0xff; 
  buffer[12]=DEFAULT_PRIORITY;
  if(lnp_assured_write(buffer,13,rcxaddr,srcport)) {
    fputs("error creating program\n",stderr);
    return -1;
  }

  // relocation target address in relocate_to
  //
  lx_relocate(&lx,relocate_to);

  lnp_download(&lx);

#if 0
    if(verbose_flag)
      fputs("run\n",stderr);
    buffer[0]=CMDrun;
    buffer[1]=prog; //       prog 0
    if(lnp_assured_write(buffer,2,rcxaddr,srcport)) {
      fputs("error running program\n",stderr);
      return -1;
    }
#endif
      
  return 0;
}
