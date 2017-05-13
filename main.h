/* 
   Copyright (C)  2008 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp. 
                  main developer: HP Wei <hp@rentec.com>
     Following the suggestion and the patch by Clint Byrum <clint@careercast.com>,
     I added more control to selectively print out messages.
     The control is done by the statement 'if (version >= n)'

   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   This file was modified in 2001 and later from files in the program 
   multicaster copyrighted by Aaron Hillegass as found at 
   <http://sourceforge.net/projects/multicaster/>

   Copyright (C)  2000 Aaron Hillegass <aaron@classmax.com>

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

#include        <dirent.h>
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
#include        <sys/times.h>
#include        <limits.h>

#define     VERSION        "4.0.1"

/* logic values */
#define     FALSE   0
#define     TRUE    1
#define     FAIL    (FALSE)
#define     SUCCESS (TRUE)
#define     GOOD_EXIT 0
#define     BAD_EXIT  -1

/* Ports and addresses */
#define     PORT           8888             /* for multicast */
#define     FLOW_PORT      (PORT-1)         /* for flow-control */
#define     MCAST_ADDR     "239.255.67.92"
#define     MCAST_TTL      1
#define     MCAST_LOOP     FALSE
#define     MCAST_IF       NULL

/* 
   Handling socket's receive buffer on the target machine:
   if the available data size in the receiveing buffer is larger
   than TOO_MUCH then a TOO_FAST complaint is triggered.
   The master will then sleep for USEC_TO_IDLE 
   Currently, this is not effective. 
*/
#define     TOO_FAST_LIMIT (TOTAL_REC_PAGE / 2)  /* if half is full, then too fast */
#define     TOO_MUCH       (TOO_FAST_LIMIT * PAGE_BUFFSIZE) 
#define     USEC_TO_IDLE   1000000               

/* TIMING stuff */
#define     FAST           5000      /* usec */
#define     DT_PERPAGE     6000      /* usec */
#define     FACTOR         90        /* interpage interval = FACTOR * DT_PERPAGE or DT_PERPAGE*/
#define     SECS_FOR_KILL  30        /* time(sec) allowed for 'kill -9 pid' to finish */

/* time for the master to wait for the acknowledgement */
#define     ACK_WAIT_PERIOD 1        /* secs (from time()); */
#define     ACK_WAIT_TIMES  60       /* wait for this many periods */

#define     SICK_RATIO     (0.9)
#define     SICK_THRESHOLD (50)     /* SICK FOR such many TIMES is really sick */

/* max wait time for write() a page of PAGE_SIZE -- 100 msec */
#define     WRITE_WAIT_SEC  0
#define     WRITE_WAIT_USEC 100000

#define     SET_MON_WAIT_TIMES    6000 /* time = this number * FAST */
#define     NO_FEEDBACK_COUNT_MAX 10
#define     SWITCH_THRESHOLD      50 /* to avoid switching monitor too frequently
                                        because of small diff in missing_pages */

/* complaints */
#define     TOO_FAST       100
#define     OPEN_OK        200
#define     CLOSE_OK       300
#define     MISSING_PAGE   400
#define     MISSING_TOTAL  500
#define     EOF_OK         600
#define     SIT_OUT        700
#define     PAGE_RECV      800
#define     MONITOR_OK     900

/* Sizes */
/* 20060427: removed size_t which is arch-dependent */
#define     PAGE_SIZE      64512    
#define     HEAD_SIZE      (sizeof(int) + 2 * sizeof(int) + 2 * sizeof(int)) 
#define     PAGE_BUFFSIZE  (PAGE_SIZE + HEAD_SIZE)
#define     TOTAL_REC_PAGE 20 /* change to 4 in case hit the OS limit in buf size */

#define     FLOW_HEAD_SIZE (sizeof(int)*4)
#define     FLOW_BUFFSIZE  (PAGE_SIZE+FLOW_HEAD_SIZE)
#define     MAX_NPAGE      (PAGE_SIZE / sizeof(int))
/* 
   Modes and command codes:
   The numerical codes are also the index to retrieve the command names 
   for printing in complaints.c
*/
#define     TIMED_OUT          0
#define     TEST               1
#define     SENDING_DATA       2
#define     RESENDING_DATA     3
#define     OPEN_FILE_CMD      4
#define     EOF_CMD            5
#define     CLOSE_FILE_CMD     6
#define     CLOSE_ABORT_CMD    7
#define     ALL_DONE_CMD       8
#define     SELECT_MONITOR_CMD 9
#define     NULL_CMD           10

/* machine status ----- for caster */
#define     MACHINE_OK_MISSING_PAGES '\2' 
#define     MACHINE_OK     '\1'
#define     NOT_READY      '\0'

#define     BAD_MACHINE    '\1'    
#define     GOOD_MACHINE   '\0'

#define     FILE_RECV      '\1'
#define     NOT_RECV       '\0'

/* representation of all-targets for sends */
#define     ALL_MACHINES   -1

/* PAGE STATUS */
#define     MISSING        '\0'
#define     RECEIVED       '\1'

/* MACHINE STATE ----- for catcher */
#define     IDLE_STATE       0
#define     GET_DATA_STATE   1
#define     DATA_READY_STATE 2
#define     SICK_STATE       3

/* 
   The following two are info to be packed into
   meta data to represent either file or directory deletion.
*/
/* SPECIAL # of PAGES to signal deleting action */
#define     TO_DELETE        (-1)

/* temporary file name prefix for transfering to */
#define     TMP_FILE       "mrsync."

#include "proto.h"

#endif
