# Makefile - keep the upper hand.

TARGET1=dll
SRCS1=loader.c lx.c
OBJS1=$(SRCS1:.c=.o)

TARGET2 = lnptest
SRCS2 = lnptest.c
OBJS2=$(SRCS2:.c=.o)

LIBS= -llnp

CINC = -I../liblnp
CFLAGS=-O2 -Wall $(CINC)
LDFLAGS = -L../liblnp

MAKEDEPEND = gcc -M $(CINC)

all:  $(TARGET1) $(TARGET2)
	
clean:
	rm -f *.o *~ *.bak $(TARGET1) $(TARGET2)

realclean: clean
	rm -f .depend* .kdbg*

depend:
	$(MAKEDEPEND) $(SRCS1) $(SRCS2) > .depend
	
	
$(TARGET1):  $(OBJS1)
	gcc -o $(TARGET1) $(OBJS1) $(LDFLAGS) $(LIBS)

$(TARGET2):  $(OBJS2)
	gcc -o $(TARGET2) $(OBJS2) $(LDFLAGS) $(LIBS)

%.o: %.c
	gcc -o $*.o -c $*.c $(CFLAGS)
  
