/*
 *
 *  The Original Code is legOS code, released October 2, 1999.
 *
 *  The Initial Developer of the Original Code is Markus L. Noga.
 *  Portions created by Markus L. Noga are Copyright (C) 1999
 *  Markus L. Noga. All Rights Reserved.
 *
 *  Contributor(s):
 */

#ifndef RCXTTY_H
#define RCXTTY_H

#define LOCKDIR "/var/lock"
#define LOCKNAME "LCK.."

// initialize RCX communications port
int tty_init(int highspeed,int nolock, const char *tty);

void tty_exit(void);

#endif
