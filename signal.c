/*  
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
		  The code in this file is copied from 
                  Richard Stevens' book
                  "UNIX Network Programming" Chap.22.3

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
*/

#include "signal.h"

Sigfunc * signal(int signo, Sigfunc *func)
{
	struct sigaction	act, oact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
	} else {
#ifdef	SA_RESTART
		act.sa_flags |= SA_RESTART;     /* SVR4, 44BSD */
#endif
	}
	if (sigaction(signo, &act, &oact) < 0)
		return(SIG_ERR);
	return(oact.sa_handler);
}
/* end signal */

Sigfunc * Signal(int signo, Sigfunc *func)	/* for our signal() function */
{
	Sigfunc	*sigfunc;

	if ( (sigfunc = signal(signo, func)) == SIG_ERR)
		perror("signal error");
	return(sigfunc);
}

int Fcntl(int fd, int cmd, int arg)
{
	int	n;

	if ( (n = fcntl(fd, cmd, arg)) == -1)
		perror("fcntl error");
	return(n);
}

int Ioctl(int fd, int request, void *arg)
{
	int		n;

	if ( (n = ioctl(fd, request, arg)) == -1)
		perror("ioctl error");
	return(n);	/* streamio of I_LIST returns value */
}

void Sigemptyset(sigset_t *set)
{
	if (sigemptyset(set) == -1)
		perror("sigemptyset error");
}

void Sigaddset(sigset_t *set, int signo)
{
	if (sigaddset(set, signo) == -1)
		perror("sigaddset error");
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
	if (sigprocmask(how, set, oset) == -1)
		perror("sigprocmask error");
}
