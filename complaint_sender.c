/*  
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp.
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

#include "main.h"

/* complaint_send_socket */
int                complaint_fd;
#ifndef IPV6
struct sockaddr_in complaint_addr;
#else
struct sockaddr_in6 complaint_addr;
#endif

extern int         my_FLOW_PORT;
extern int         verbose;

/* send buffer */
char               complaint_buffer[FLOW_BUFFSIZE];  /* (ccode, cfile, cpage) */
/* 
   if ccode is related to missing page
      *cfile_ptr = which file -- for the missing page
      *cpage_ptr = which page -- for the missing page
   else if ccode is one of the cmd
      *cfile_ptr = machine ID number
      *cpage_ptr = 0
*/
int                *ccode_ptr;  /* complain code ---- see main.h */
int                *cmid_ptr;   /* which machine -- for missing page */
int                *cpage_ptr;  /* which page -- for missing page */

/*----------------------------------------------------------
  init_complaint_sender initializes the buffer to allow the 
  catcher to send complaints back to the sender.

  ret_address of sender to whom we will complain 
  is determined when we receive the first UDP data
  in read_handle_page() in page_reader.c
  ----------------------------------------------------------*/
void init_complaint_sender()    /* (struct sockaddr_in *ret_addr) */
{
  /* ret_addr is sent by master, in network-byte-order */
  if (verbose>=2)
    fprintf(stderr, "in init_complaint_sender\n");
  /* init the send_socket */
  complaint_fd = complaint_socket(&complaint_addr, my_FLOW_PORT);

  /* set up the pointers so we know where to put complaint_data */
  ccode_ptr = (int *) complaint_buffer;
  cmid_ptr  = (int *)(ccode_ptr + 1);
  cpage_ptr = (int *)(cmid_ptr + 1);
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

  It is also used for sending back acknoledgement.
  complaint -- the ack code defined in main.h in the same complaint section.
  file      -- the machine ID number
  page      -- no use. Set it to 0
  ------------------------------------------------------------------------*/	
void send_complaint(int complaint, int mid, int page)
{
  /* fill in the complaint data */
  /* 20060323 add converting to network byte-order before sending out */
  *ccode_ptr = htonl(complaint);
  *cmid_ptr  = htonl(mid);
  *cpage_ptr = htonl(page);

  /* send it */
  if(sendto(complaint_fd, complaint_buffer, FLOW_BUFFSIZE, 0,
	    (const struct sockaddr *)&complaint_addr, 
	    sizeof(complaint_addr)) < 0) {
    perror("Sending complaint\n");
  }
  if (verbose>=2)
    printf("Sent complaint:code=%d mid=%d page=%d\n", complaint, mid, page);
}
