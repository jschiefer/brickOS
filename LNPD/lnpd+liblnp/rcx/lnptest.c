//
// send & receive lnp Packets
//


// terminate this program with the PRGM-Button !!!

#include <lnp.h>
#include <conio.h>
#include <sys/irq.h>
#include <sys/h8.h>
#include <tm.h>
#include <dkey.h>
#include <unistd.h>
#include <lnp-logical.h>
#include <sys/lnp-logical.h>

#define MY_PORT 2

#define DEST_HOST 0x8
#define DEST_PORT 0x7
#define DEST_PORT_2 0x8
#define DEST_ADDR ( DEST_HOST << 4 | DEST_PORT )
#define DEST_ADDR_2 ( DEST_HOST << 4 | DEST_PORT_2 )
#define LEN_1 100
#define LEN_2 77
#define LEN_3 13

static unsigned char rx_count;
static unsigned char tx_count_1,tx_count_2,tx_count_3;
static int done,sender_1_done,sender_2_done,sender_3_done;

wakeup_t prgmKeyCheck(wakeup_t data)
{
    return dkey == KEY_PRGM;
}

wakeup_t countchanged(wakeup_t data)
{
	wakeup_t value = 0;
	static unsigned char oldrx,oldtx;
	
	if ( (tx_count_1 + tx_count_2 + tx_count_3) != oldtx )
	{
		oldtx = tx_count_1 + tx_count_2 + tx_count_3;
		value = 1;
	}
	if (rx_count != oldrx)
	{
		oldrx = rx_count;
		value = 1;
	}
	return value;
}

wakeup_t countOrKey(wakeup_t dummy)
{
	return ( countchanged(0) || prgmKeyCheck(0) );
}

wakeup_t childTest(wakeup_t dummy)
{
	return ( sender_1_done && sender_2_done && sender_3_done );
}

void show_counters(void)
{
    cputw( ( ( tx_count_1 + tx_count_2 + tx_count_3) << 8) | rx_count );
}

void packet_handler(const unsigned char* data,unsigned char length, unsigned char src)
{
	++rx_count;
}

int sender_1(int argc, char *argv[])
{
    unsigned char data[LEN_1];
    int i;
    unsigned char len = LEN_1;
    signed char result;

    for (i=0;i<sizeof(data);i++) data[i] = i;

    while(!done)
    {
     	data[0]=tx_count_1;
        result = lnp_integrity_write(data,len);
        if (result == TX_IDLE) ++tx_count_1;
        msleep(10);
    }
    sender_1_done = 1;
    return 0;
}

int sender_2(int argc, char *argv[])
{
    unsigned char data[LEN_2];
    int i;
    unsigned char len = LEN_2;
    signed char result;

    for (i=0;i<sizeof(data);i++) data[i] = i;

    while(!done)
    {
     	data[0]=tx_count_2;
        result = lnp_addressing_write(data,len ,DEST_ADDR,MY_PORT);
        if (result == TX_IDLE) ++tx_count_2;
        msleep(10);
    }
	sender_2_done = 1;
    return 0;
}

int sender_3(int argc, char *argv[])
{
    unsigned char data[LEN_3];
    int i;
    unsigned char len = LEN_3;
    signed char result;

    for (i=0;i<sizeof(data);i++) data[i] = i;

    while(!done)
    {
     	data[0]=tx_count_3;
        result = lnp_addressing_write(data,len ,DEST_ADDR_2,MY_PORT);
        if (result == TX_IDLE) ++tx_count_3;
        msleep(10);
    }
	sender_3_done = 1;
    return 0;
}

int main(int argc, char *argv[])
{
    lnp_logical_range(0);
    lcd_clear();
    cputs("wait");

    lnp_addressing_set_handler(MY_PORT, packet_handler);
    show_counters();

	execi(sender_1,0,NULL,PRIO_NORMAL,DEFAULT_STACK_SIZE);
	execi(sender_2,0,NULL,PRIO_NORMAL,DEFAULT_STACK_SIZE);
	execi(sender_3,0,NULL,PRIO_NORMAL,DEFAULT_STACK_SIZE);
	
	while ( 1 )
	{
		wait_event(countOrKey,0);
		show_counters();
        if (prgmKeyCheck(0)) break;
    }

    done = 1;

    wait_event(childTest,0);
    lnp_addressing_set_handler(MY_PORT,LNP_DUMMY_ADDRESSING);
    cputs("done");
    return 0;
}

