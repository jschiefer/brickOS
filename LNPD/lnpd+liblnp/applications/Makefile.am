# tell automake what need to be built herein

bin_PROGRAMS = lnpdll lnptest lnptest2

lnpdll_SOURCES = loader.c \
              lx.c lx.h

lnptest_SOURCES = lnptest.c

lnptest2_SOURCES = lnptest2.c

INCLUDES = -I$(top_srcdir)/lnpd+liblnp/liblnp

LDFLAGS = -L$(top_builddir)/lnpd+liblnp/liblnp

LIBS= -llnp

