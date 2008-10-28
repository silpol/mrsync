/*  
   Copyright (C)  2008 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
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

int seq = 0;

/* send buffer */
char               complaint_buffer[FLOW_BUFFSIZE];  
int                *ccode_ptr;  /* complain code ---- see main.h */
int                *cmid_ptr;   /* which machine*/
int                *cfile_ptr;  /* which file -- for missing page */
int                *npage_ptr;  /* # of pages -- for missing page */
int                *pArray_ptr; /* missing page arrary */
int                *fill_ptr;   /* point to next array element */

/* ----------------------------------------------------------------
   routines to fill in pArray with the missing page indexes
   --------------------------------------------------------------- */
void fill_in_int(int i)
{
  *fill_ptr++ = htonl(i);
}

void init_fill_ptr()
{
  fill_ptr = pArray_ptr;
}

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
  cfile_ptr = (int *)(cmid_ptr + 1);
  npage_ptr = (int *)(cfile_ptr + 1);
  pArray_ptr= (int *)(npage_ptr + 1);
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

  The major use is to tell master machine which pages of which file
  needs to be re-transmitted.
  complaint -- the complain code defined in main.h
  mid       -- machine id
  file      -- the file index
  npage     -- # of missing pages 
  followed by an array of missing page index [ page_1, page_2, ... ]

  It is also used for sending back acknoledgement.
  complaint -- the ack code defined in main.h in the same complaint section.
  mid       -- machine id
  file      -- which file
  page      -- seq number (out of seq complaints will be ignored by the catcher)
  ------------------------------------------------------------------------*/	
void send_complaint(int complaint, int mid, int page, int file)
{
  /* fill in the complaint data */
  /* 20060323 add converting to network byte-order before sending out */
  int bytes;
  *ccode_ptr = htonl(complaint);
  *cmid_ptr  = htonl(mid);
  *cfile_ptr = htonl(file);
  if (complaint==MISSING_PAGE || complaint==MISSING_TOTAL) {
    *npage_ptr = htonl(page);
  } else {
    *npage_ptr = htonl(seq++);
  }

  bytes = (complaint==MISSING_PAGE) ? ((char*)fill_ptr - (char*)ccode_ptr) 
    : (char*)pArray_ptr - (char*)ccode_ptr;  

  /* send it */
  if(sendto(complaint_fd, complaint_buffer, bytes, 0,
	    (const struct sockaddr *)&complaint_addr, 
	    sizeof(complaint_addr)) < 0) {
    perror("Sending complaint\n");
  }
  if (verbose>=2)
    printf("Sent complaint:code=%d mid=%d page=%d file=%d bytes=%d\n", 
	   complaint, mid, page, file, bytes);
}








