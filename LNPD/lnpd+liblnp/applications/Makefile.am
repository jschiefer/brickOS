# tell automake what need to be built herein

bin_PROGRAMS = lnpdll lnptest

lnpdll_SOURCES = loader.c \
              lx.c lx.h

lnptest_SOURCES = lnptest.c

INCLUDES = -I$(top_srcdir)/lnpd+liblnp/liblnp

LDFLAGS = -L$(top_builddir)/lnpd+liblnp/liblnp

LIBS= -llnp

