#include <lnp.h>
#include <conio.h>
#include <string.h>
#include <lnp-logical.h>
#include <dkey.h>

#define MY_PORT 2
#define DEST_HOST 0x8
#define DEST_PORT 0x7
#define DEST_ADDR ( DEST_HOST << 4 | DEST_PORT )

unsigned char buf[3];
unsigned char len = 3;

void printInt(int i)
{
	int result;

	buf[0] = 'i';
    memcpy(buf + 1, &i, 2);
    result = lnp_addressing_write( buf, len, DEST_ADDR, MY_PORT);
}

wakeup_t prgmKeyCheck(wakeup_t data)
{
    return dkey == KEY_PRGM;
}

int main ( int argc, char **argv )
{
	int c=0;

    lnp_logical_range ( 0 );
    while( 1 ) {
        if( prgmKeyCheck( 0 ) ) break;
        printInt( c );
        cputw( c++ );
	    msleep ( 200 );
    }
    cputs ( "done" );
	sleep(2);	// show done for 2 sec then clear display...
    lcd_clear();
    return 0;
}
