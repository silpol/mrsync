/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com> 
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>

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

#ifndef	__main_h
#define	__main_h

#include        <time.h>
#include        <utime.h>
#include        <sys/socket.h>
#include        <sys/types.h>
#include        <sys/un.h>
#include        <stdio.h>
#include        <stdlib.h>
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>
#include	<fcntl.h>	/* for nonblocking */
#include	<sys/ioctl.h>
#include	<netdb.h>
#include	<signal.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/stat.h>	/* for S_xxx file mode constants */
#include	<sys/uio.h>	/* for iovec{} and readv/writev */
#include	<unistd.h>
#include	<sys/wait.h>
#include	<sys/time.h>	/* timeval{} for select() */

#define     VERSION "3.1.0"

/* logic values */
#define     FALSE     0
#define     TRUE      1
#define     FAIL      (FALSE)
#define     SUCCESS   (TRUE)
#define     GOOD_EXIT 0
#define     BAD_EXIT  -1

/* Ports and addresses */
#define     PORT           7900             /* for multicast */
#define     FLOW_PORT      (PORT-1)         /* for flow-control */
#define     MCAST_ADDR     "239.255.67.200"
#define     MCAST_TTL      1
#define     MCAST_LOOP     FALSE
#define     MCAST_IF       NULL

#define     REMOTE_SHELL   "rsh"

#define     NO_FEEDBACK_COUNT_MAX 5
#define     USEC_TO_IDLE   1000000               

/* Speed stuff */
#define     FAST           100       /* usec */
#define     DT_PERPAGE     8000      /* usec time interval between pages */
#define     FACTOR         50

/* time for the master to wait for the acknowledgement */
#define     ACK_WAIT_PERIOD 1        /* secs (from time()); */
#define     ACK_WAIT_TIMES  60       /* wait for this many periods */

/* complaints */
#define     TOO_FAST       100
#define     SEND_AGAIN     200
#define     START_OK       300
#define     MISSING_PAGE   500
#define     LAST_MISSING   600
#define     EOF_OK         700
#define     PAGE_RECV      800

#define     FLOW_BUFFSIZE  (2 * sizeof(int))

#define     PAGE_SIZE      64512 /* max page_size allowed */ 
#define     HEAD_SIZE      (3 * sizeof(int))
#define     PAGE_BUFFSIZE  (PAGE_SIZE + HEAD_SIZE)
#define     TOTAL_REC_PAGE 20 /* 31 20 */

/* Modes */
#define     TIMED_OUT      0
#define     TEST           1
#define     SENDING_DATA   2
#define     RESENDING_DATA 3
#define     START_CMD      4
#define     EOF_CMD        5
#define     ALL_DONE_CMD   6
#define     NULL_CMD       7

/* machine status */
#define     MACHINE_OK     '\1'
#define     NOT_READY      '\0'

/* PAGE STATUS */
#define     MISSING        '\0'
#define     RECEIVED       '\1'

/* MACHINE STATE */
#define     IDLE_STATE       0
#define     GET_DATA_STATE   1
#define     DATA_READY_STATE 2

#define     MAX_PAGE_SIZE    64512

int    verbose;

#include "rttproto.h"

#endif
