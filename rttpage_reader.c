/*
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>  
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
                  Codes in this file are extraced and modified from page_reader.c

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

#include "rttmain.h"

/* the following is needed on Sun but not on linux */
#ifdef _SUN
#include <sys/filio.h>  
#endif

extern char *      my_MCAST_ADDR;  /* defined in rttcatcher.c */
extern int         my_PORT;
extern char *      my_IFname;
int                machineState;
int                isFirstPage = TRUE;

int                recfd;
#ifndef IPV6
struct sockaddr_in	rec_addr;
#else
struct sockaddr_in6	rec_addr;
#endif

int               *mode_ptr;        /* change from char to int */
int               *total_bytes_ptr;
int               *current_page_ptr;
char              *data_ptr;
char               rec_buf[PAGE_BUFFSIZE];

void init_page_reader()
{
  struct ip_mreq mreq;
  int rcv_size;

  machineState = IDLE_STATE;

  /* Prepare buffer */
  mode_ptr  = (int*)rec_buf;   /* hp: add the cast (int *) */
  current_page_ptr = (int *)(mode_ptr + 1);
  total_bytes_ptr = (int *)(current_page_ptr + 1);
  data_ptr = (char *)(total_bytes_ptr + 1);

    /* Set up receive socket */
  if (verbose) fprintf(stderr, "setting up receive socket\n");
  recfd = rec_socket(&rec_addr, my_PORT);

  /* Join the multicast group */
  if (Mcast_join(recfd, my_MCAST_ADDR, my_IFname, 0)<0) {
    perror("Joining Multicast Group");
  }
  
  /* Increase socket receive buffer */
  rcv_size = TOTAL_REC_PAGE * PAGE_BUFFSIZE;
  if (setsockopt(recfd, SOL_SOCKET, SO_RCVBUF, &rcv_size, sizeof(rcv_size)) < 0){
    perror("Expanding receive buffer");
  }
}


/*
  This is the heart of catcher.
  It parses the incoming pages and do proper reactions according
  to the mode (command code) encoded in the first 4 bytes in an UDP page.
  It returns the mode.

  Note: since rtt is intended to deal with one-to-one machine,
        the four-state engine as in page_reader is not used.
*/
int read_handle_page()
{
  #ifndef IPV6
  struct sockaddr_in      return_addr;
  #else
  struct sockaddr_in6     return_addr;
  #endif

  int                     bytes_read;
  socklen_t               return_len = (socklen_t)sizeof(return_addr);
  int mode_v, total_bytes_v, current_page_v;

  /* -----------receiving data -----------------*/
  if (readable(recfd) == 1) { /* there is data coming in */
    /* get data */
    bytes_read = recvfrom(recfd, rec_buf, PAGE_BUFFSIZE, 0, 
			  (struct sockaddr *)&return_addr, 
			  (socklen_t*) &return_len);

    total_bytes_v = ntohl(*total_bytes_ptr);
    if (bytes_read != total_bytes_v) return NULL_CMD;

    /* convert from network byte order to host byte order */
    mode_v = ntohl(*mode_ptr);    
    current_page_v = ntohl(*current_page_ptr);

    if (isFirstPage) {
      update_complaint_address(&return_addr);
      isFirstPage = FALSE;
    }

    /* get init wish list and return address first time only 
    if (firstTime){
      if (verbose)
	fprintf(stderr, "Initializing complaint_sender and wish_list\n");
      init_complaint_sender(&return_addr);
      firstTime = 0;
    }
    */

    /* --- process various commands */
    switch (mode_v) {
    case TEST:
      /* It is just a test packet? */
      fprintf(stderr, "********** Received test packet **********\n");
      return mode_v;
 
    case START_CMD:
      if (verbose)
	fprintf(stderr, "Start cmd received ---\n"); 
      init_missingPages(current_page_v);  /* Here: use current_page for total_pages */
      send_complaint(START_OK, 0); 
      return mode_v;

    case EOF_CMD:
      if (verbose)
	fprintf(stderr, "Check and ask for missing pages ---\n");
     
      if (ask_for_missing_page()==0) {
	/* 
	   There is no missing page.
	*/
	send_complaint(EOF_OK, 0);
      } 
      /* 
	 else
	 There are missing pages.
	 Ack has been done in ask_for_missing_page()
      */

      return mode_v;
  
    case SENDING_DATA:
    case RESENDING_DATA:
      if (verbose){
	fprintf(stderr, "Got %d bytes from page %d of %d,  mode=%d\n", 
		bytes_read - HEAD_SIZE,
		current_page_v,  get_total_pages(), mode_v); 
      }

      /* mode == SENDING_DATA, RESENDING_DATA */
      page_received(current_page_v);
      send_complaint(PAGE_RECV, 0);

      /* Yes, we read a page */
      return mode_v;
    case ALL_DONE_CMD:     /* this is presumably a good machine */
    default:
      return mode_v;
    } /* end of switch */
  } else {
    /* No,  the read timed out */
    return TIMED_OUT;
  }
}


