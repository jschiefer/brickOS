#
# lnpd - the BrickOS (formerly legos) Network Protocol Daemon
# liblnp - a library against which to build client Linux apps
# dllx - one such application which allows for program downloads to RCX via lnp
#

pkgdocdir = $(datadir)/doc/@PACKAGE@
pkgexmpldir= $(datadir)/doc/@PACKAGE@/examples

SUBDIRS = lnpd+liblnp dllx

EXTRA_DIST = AUTHORS TODO COPYING \
             ChangeLog INSTALL NEWS \
             README VERSION inst-pieces

distclean-generic:
	rm -f stamp-h.in *~ stamp-h
