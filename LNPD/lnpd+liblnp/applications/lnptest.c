//
// kleines Testprogram für linux lnp
//
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

#include "liblnp.h"

#define MY_PORT_1  7
#define MY_PORT_2  8

#define DEST_HOST 0
#define DEST_PORT 2
#define DEST_ADDR ( DEST_HOST << 4 | DEST_PORT )
#define LEN 253

void addr_handler_1(const unsigned char* data,unsigned char length, unsigned char src)
{
    char pbuf[100];

    sprintf(pbuf,">> Source:%2X Length:%u PacketNo:%u <<\n",(unsigned)src,(unsigned)length,(unsigned)data[0]);
    
    write(STDERR_FILENO,pbuf,strlen(pbuf));
}

void addr_handler_2(const unsigned char* data,unsigned char length, unsigned char src)
{
    char pbuf[100];

    sprintf(pbuf,">> Source:%2X Length:%u PacketNo:%u <<\n",(unsigned)src,(unsigned)length,(unsigned)data[0]);

    write(STDERR_FILENO,pbuf,strlen(pbuf));
}

void int_handler(const unsigned char* data,unsigned char length)
{
    char pbuf[100];

    sprintf(pbuf,">> Integrity Length:%u PacketNo:%u <<\n",(unsigned)length,(unsigned)data[0]);

    write(STDERR_FILENO,pbuf,strlen(pbuf));
}

int main(int argc, char *argv[])
{
    char data[253];
    int i;
    lnp_tx_result result;
    unsigned char len;
    int count = 0;

    for (i=0;i<sizeof(data);i++) data[i] = i;

    if ( lnp_init(0,0,0,0,0) )
    {
    	perror("lnp_init");	
    	exit(1);
    }

    else fprintf(stderr,"init OK\n");

    lnp_addressing_set_handler (MY_PORT_1, addr_handler_1);
    lnp_addressing_set_handler (MY_PORT_2, addr_handler_2);
    lnp_integrity_set_handler (int_handler);

    while (1)
    {
    	//sleep(1000);
    	//continue;
	    len = LEN; //random() % 252 + 1;
		result = lnp_addressing_write(data,len ,DEST_ADDR,MY_PORT_1);
		switch (result)
		{
        	case TX_SUCCESS:
        		printf("Tansmitted %d : %d\n",len,count++);
    			break;
    		case TX_FAILURE:
    			printf("Collision\n");
        		break;
        	default:
        		perror("Transmit error");
        		exit(1);
        }
	}

    return 0;
}
