# Makefile - keep the upper hand.

TARGET = lnpd

PARTS=rcxtty lnp logger transceiver daemon main

SRCS=$(addsuffix .c,$(PARTS))
OBJS=$(addsuffix .o,$(PARTS))

CC = gcc
LIBS=
CINC = 

CFLAGS=-g -Wall $(CINC) -D_GNU_SOURCE
MAKEDEPEND = gcc -M $(CINC)

all: $(TARGET)

clean:
	rm -f *.o *~ *.bak $(OBJS) $(TARGET)

realclean: clean
	rm -f .depend* .kdbg* gdb-*

$(TARGET):  $(OBJS)
#	-sudo chown mac $@
	gcc -o $@ $(OBJS) $(LIBS)
#	sudo chown root $@
#	sudo chmod u+s $@

%.o: %.c config.h
	gcc -o $*.o -c $*.c $(CFLAGS)

depend:
	$(MAKEDEPEND) $(SRCS) > .depend

#		
# dependencies
#
ifneq ($(wildcard .depend),)
include .depend
endif
