#  Example makefile to install website via ssh to sourceforge
#  Adjust the LOGIN_NAME value before use
#
#  post to web sequence:
#    {edit the files}
#    {cvs commit the changes}
#    make rls   #  to post the content
#
LOGIN_NAME=stephmo
FLAGFIL=.lastInstall
COPYDIR=LOCAL_COPY_OF_WEB

all:
	@echo "useful targets are: rls"

ensureLocalCopyDirExists:
	@if test ! -d $(COPYDIR); then \
		(set -x;mkdir -p $(COPYDIR)); \
	fi

ensureFlagExists:
	@if test ! -f $(FLAGFIL); then \
		(set -x;touch -t 0001010001 $(FLAGFIL)); \
	fi

clearOutGzip:
	@if [ -f brickos-web.tar.gz ]; then \
		mv brickos-web.tar.gz $(COPYDIR); \
	fi

rls: ensureFlagExists clearOutGzip
	@(export CURDIR=`pwd`; FLAG=$$CURDIR/$(FLAGFIL); \
	for DIR in `find . -type d | egrep -v "CVS|$(COPYDIR)"`; do \
	   cd $$DIR; \
	   echo "Installing files found in $$DIR:"; \
	   for FIL in `find . -type f -maxdepth 1`; do \
	   	if [[ $$FIL -nt $$FLAG ]]; then \
	   		scp $$FIL $(LOGIN_NAME)@brickos.sourceforge.net:/home/groups/b/br/brickos/htdocs/$$DIR; \
	   	fi; \
	   done; \
	   cd -; \
	done)
	@sleep 2
	@touch $(FLAGFIL)
	@echo "Install is Done"

get: ensureLocalCopyDirExists
	ssh $(LOGIN_NAME)@brickos.sourceforge.net "cd /home/groups/b/br/brickos;tar --gzip -cf ~/brickos-web.tar.gz htdocs"
	scp $(LOGIN_NAME)@brickos.sourceforge.net:brickos-web.tar.gz .
	@(cd $(COPYDIR); \
	tar --gzip -xf ../brickos-web.tar.gz; \
	cd -)

.PHONEY: rls all
