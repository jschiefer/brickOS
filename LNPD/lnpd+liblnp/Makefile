#
# lnpd & liblnp
# a Linux Interface to BrickOS
#

SUBDIRS = lnpd liblnp applications

all depend clean realclean:
	for i in $(SUBDIRS) ; do make -C $$i $@ || exit -1 ; done

