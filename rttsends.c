/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com> 
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
                  codes in this file are extracted and modified from sends.c

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
#include <net/if.h>
#ifdef _SUN
#include <sys/sockio.h>  /* define SIOCGIFADDR */
#else
#include <linux/sockios.h>
#endif

extern char *      my_MCAST_ADDR;  
extern int         my_PORT;        
extern int         my_TTL;
extern int         my_LOOP;
extern char *      my_IFname;      

/* buffer for sending (same structure as those in sends.c */
int          *mode_ptr; /* char type would cause alignment problem on Sparc */
int          *current_page_ptr;
int          *total_bytes_ptr;
char         *data_ptr;
char          send_buff[PAGE_BUFFSIZE];

int pageSize;

/* Send socket */
int send_fd;
#ifndef IPV6
struct sockaddr_in send_addr;
#else
struct sockaddr_in6 send_addr;
#endif

/*
  set_mode sets the caster into a new mode.
  modes are defined in main.h:
*/
void set_mode(int new_mode)
{
  *mode_ptr = htonl(new_mode);
}


/* init_sends initializes the send buffer */
void init_sends(int npagesize)
{
  pageSize = (npagesize>PAGE_SIZE) ? PAGE_SIZE : npagesize;

  mode_ptr = (int *)send_buff;   /* hp: add (int*) */
  current_page_ptr = (int *) (mode_ptr + 1);
  total_bytes_ptr = (int *)(current_page_ptr + 1);
  data_ptr = (char *)(total_bytes_ptr + 1);

  send_fd = send_socket(&send_addr, my_MCAST_ADDR, my_PORT);

    /******* change MULTICAST_IF ********/
  if (my_IFname != NULL && mcast_set_if(send_fd, my_IFname, 0)<0) 
    perror("init_sends(): when set MULTICAST_IF\n");
  
  /* set multicast_ttl such that UDP can go to 2nd subnetwork */  
  if (mcast_set_ttl(send_fd, my_TTL) < 0) 
    perror("init_sends(): when set MULTICAST_TTL\n");
  
  /* disable multicast_loop such that there is no echo back on master */
  if (mcast_set_loop(send_fd, my_LOOP) < 0) 
    perror("init_sends(): when set MULTICAST_LOOP\n");  

  /* put dummy contents into send (UDP) buffer */
  memset(data_ptr, 1, PAGE_SIZE);
}

/*
  send_buffer will send the buffer with the file information
  out to the socket connection with the catcher.
*/
int send_buffer(int bytes_read)
{
    /* Else send the data */
    if(sendto(send_fd, send_buff, bytes_read + HEAD_SIZE, 
	      0, (const struct sockaddr *)&send_addr, sizeof(send_addr)) < 0) {
      perror("Sending packet");
      exit(1);
    }
    return (1);
}

/*
  send_page takes a page from the current file and controls
  sending it out the socket to the catcher.  It calls send_buffer
  to do the actuall call to sendto.
*/
int send_page(int page)
{
  if (verbose>=2) fprintf(stderr, "in send_page\n");
  *total_bytes_ptr = htonl(pageSize+HEAD_SIZE);
  *current_page_ptr = htonl(page);
  
  return send_buffer(pageSize);
}


void send_cmd(int code, int pages) 
{  
  *mode_ptr = htonl(code);
  *current_page_ptr = htonl(pages);  
  *total_bytes_ptr = htonl(HEAD_SIZE);

  send_buffer(0);
}


void send_all_done_cmd()
{
  *mode_ptr = htonl(ALL_DONE_CMD);  
  *current_page_ptr = 0; 
  *total_bytes_ptr = htonl(HEAD_SIZE) ;
  
  send_buffer(0);
  if (verbose) fprintf(stderr, "(ALL DONE)\n");
}
