#include <liblnp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>

#define MY_PORT 7

void addr_handler(const unsigned char* data,unsigned char length,
				unsigned char src)
{
    switch(data[0]) {
	      case 's':
	           printf( "%s\n", &data[1] );
	           break;
          case 'i':
	           printf( "%d\n", data[1] * 256 +  data[2] );
	           break;
          default:
               printf( "unknown type\n" );
     }
     fflush( stdout );
}

int main ( int argc, char **argv ) {
    if ( lnp_init ( 0, 0, 0, 0, 0 ) ) {
       perror ( "lnp_init" );
       exit(1);
    } else {
       printf ( "init OK\n" );
    }

    lnp_addressing_set_handler (MY_PORT, addr_handler );

    while ( 1 ) {
    };

    return 0;
}
