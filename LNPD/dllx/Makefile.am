# tell automake what need to be built herein

bin_PROGRAMS = lnpdllx

lnpdllx_SOURCES = loader2.c lx.c lx.h

INCLUDES = -I$(top_srcdir)/lnpd+liblnp/liblnp

LDFLAGS = -L$(top_builddir)/lnpd+liblnp/liblnp

LIBS= -llnp
 
man_MANS = lnpdllx.1

EXTRA_DIST = lnpdllx.1
