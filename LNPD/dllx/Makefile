# Makefile - keep the upper hand.

#adjust to your setup
LIBLNPDIR=../lnpd+liblnp/liblnp
LIBLNPINC=../lnpd+liblnp/liblnp

TARGET1=dllx
SRCS1=loader2.c lx.c
OBJS1=$(SRCS1:.c=.o)

LIBS= -llnp

CINC = -I. -I$(LIBLNPINC)
CFLAGS=-g -Wall $(CINC)
LDFLAGS = -L$(LIBLNPDIR)

MAKEDEPEND = gcc -M $(CINC)

all:  $(TARGET1)
	
clean:
	rm -f *.o *~ *.bak $(TARGET1)

realclean: clean
	rm -f .depend* .kdbg*

depend:
	$(MAKEDEPEND) $(SRCS1) > .depend
	
$(TARGET1):  $(OBJS1)
	gcc -o $@ $+ $(LDFLAGS) $(LIBS)

%.o: %.c
	gcc -o $*.o -c $*.c $(CFLAGS)
  
