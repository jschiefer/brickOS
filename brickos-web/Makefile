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

all:
	@echo "useful targets are: rls"

ensureFlagExists:
	@if test ! -f $(FLAGFIL); then \
		(set -x;touch -t 0001010001 $(FLAGFIL)); \
	fi

rls: ensureFlagExists
	@(export CURDIR=`pwd`; FLAG=$$CURDIR/$(FLAGFIL); \
	for DIR in `find . -type d | egrep -v CVS`; do \
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

.PHONEY: rls all
