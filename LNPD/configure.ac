#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(lnpd, 0.9.0)
AC_CONFIG_AUX_DIR(inst-pieces)
AC_CONFIG_SRCDIR(configure.ac)
AM_CONFIG_HEADER(config.h)

# Checks for system type
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)

# Not yet used...
AC_DEFINE_UNQUOTED(BUILDDATE,"`date -u`",[Date])
BUILDDATE="`date -u`"
AC_SUBST(BUILDDATE)

# Build static libs only
#AC_DISABLE_SHARED

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL
AM_PROG_LIBTOOL

# Checks for libraries.
# FIXME: Replace `main' with a function in `-lc':
AC_CHECK_LIB([c], [main])
# FIXME: Replace `main' with a function in `-llnp':
AC_CHECK_LIB([lnp], [main])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h memory.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_VOLATILE

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit gethostbyname gettimeofday inet_ntoa memset select socket strchr strdup strerror strrchr strtoul])

AC_CONFIG_FILES([Makefile
                 lnpd+liblnp/Makefile
                 lnpd+liblnp/liblnp/Makefile
                 lnpd+liblnp/lnpd/Makefile
                 lnpd+liblnp/applications/Makefile
                 lnpd+liblnp/rcx/Makefile
                 dllx/Makefile])
AC_OUTPUT


