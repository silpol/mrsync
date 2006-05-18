/*
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>  
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
                  Codes in this file are extracted and modified from complaint_sender.c

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

/* send socket */
int                complaint_fd;
#ifndef IPV6
struct sockaddr_in complaint_addr;
#else
struct sockaddr_in6 complaint_addr;
#endif

extern int         my_FLOW_PORT;

/* send buffer */
char               complaint_buffer[FLOW_BUFFSIZE];
int               *ccode_ptr;  /* change from char to int -- mem alignment */
int               *cpage_ptr;

/*----------------------------------------------------------
  init_complaint_sender initializes the buffer to allow the 
  catcher to send complaints back to the sender.

  ret_address of sender to whom we will complain 
  is determined when we receive the first UDP data
  in read_handle_page() in page_reader.c
  ----------------------------------------------------------*/
void init_complaint_sender()
{
  if (verbose)
    fprintf(stderr, "in init_complaint_sender\n");
  
  /* init the send_socket */
  complaint_fd = complaint_socket(&complaint_addr, my_FLOW_PORT);

  ccode_ptr = (int *) complaint_buffer;
  cpage_ptr = (int *)(ccode_ptr + 1);
}

#ifndef IPV6
void update_complaint_address(struct sockaddr_in *sa)
{
  sock_set_addr((struct sockaddr *) &complaint_addr, 
		sizeof(complaint_addr), (void*)&sa->sin_addr);
}
#else
void update_complaint_address(struct sockaddr_in6 *sa)
{
  sock_set_addr((struct sockaddr *) &complaint_addr, 
		sizeof(complaint_addr), (void*)&sa->sin6_addr);
}
#endif

/*------------------------------------------------------------------------
  send_complaint fills the complaint buffer and send it through our socket
  back to the sender

  The major use is to tell master machine which page of which file
  needs to be re-transmitted.
  complaint -- the complain code defined in main.h
  file      -- the file index
  page      -- page index
  ------------------------------------------------------------------------*/	
void send_complaint(int complaint, int page)
{
  /* fill in the complaint data */
  /* 20060323 add converting to network byte-order before sending out */
  *ccode_ptr = htonl(complaint);
  *cpage_ptr = htonl(page);

  /* send it */
  if( sendto(complaint_fd, complaint_buffer, FLOW_BUFFSIZE, 0,
	     (const struct sockaddr *)&complaint_addr, 
	     sizeof(complaint_addr)) < 0) {
    perror("Sending complaint\n");
  }
  if (verbose)
    printf("Sent complaint:code=%d page=%d\n", complaint, page);
}
