#  Example makefile to install website via ssh to sourceforge
#  Adjust the SF_LOGIN value before use
#
#  post to web sequence:
#    {edit the files}
#    {cvs commit the changes}
#    make release   #  to post the content
#
SF_LOGIN=stephmo

all:

rls:
	scp -r `/bin/ls | egrep -v "Makefile|CVS|Root|Repository|Entries"` \
		$(SF_LOGIN)@brickos.sourceforge.net:/home/groups/b/br/brickos/htdocs

.PHONEY: rls
