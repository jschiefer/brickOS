# Makefile - keep the upper hand.

REV_MAJOR = 0
REV_MINOR = 0
REV_VERSION = 1

LIBNAME = liblnp.so
LIBSTATIC = liblnp.a

LIBSONAME = $(LIBNAME).$(REV_MAJOR)
LIBTARGET = $(LIBSONAME).$(REV_MINOR).$(REV_VERSION)

LIBPARTS = stub

LIBSRCS=$(addsuffix .c,$(LIBPARTS))
LIBOBJS=$(addsuffix .o,$(LIBPARTS))

LIBINCLUDES  = -I.

LIBCFLAGS = -g -Wall $(LIBINCLUDES) -D_GNU_SOURCE -fPIC
LIBLDFLAGS = -shared -Wl,-soname,$(LIBSONAME)
LIBLDLIBES = -lc

MAKEDEPEND = gcc -M $(LIBINCLUDES)

all: $(LIBTARGET) $(LIBSTATIC)

clean:
	rm -f *.o $(LIBOBJS) $(LIBNAME)* $(LIBSTATIC)

realclean: clean
	rm -f .depend* *~ *.bak .kdbg*

$(LIBTARGET):  $(LIBOBJS)
	gcc $(LIBLDFLAGS) -o $@ $(LIBOBJS) $(LIBLDLIBES)
	ln -fs $@ $(LIBNAME)
	ln -fs $@ $(LIBSONAME)

$(LIBSTATIC): $(LIBOBJS)
	rm -f $@
	ar -rcs $@ $+
	
stub.o: stub.c
	gcc -o $@ -c $< $(LIBCFLAGS)
	
depend:
	$(MAKEDEPEND) $(LIBSRCS) > .depend

#		
# include dependencies, if existing
#
ifneq ($(wildcard .depend),)
include .depend
endif
