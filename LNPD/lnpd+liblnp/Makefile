#
# lnpd & liblnp
# a LinuX Interface to legOS
#

SUBDIRS = lnpd liblnp applications

all depend clean realclean:
	for i in $(SUBDIRS) ; do make -C $$i $@ || exit -1 ; done

