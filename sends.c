/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
     Following the suggestion by Robert Dack <robd@kelman.com>,
     I added the option to change the default IP address for multicasting
     and the PORT for flow control.  See mrsync.py for the new options.

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
#include <net/if.h>
#ifdef _SUN
#include <sys/sockio.h>  /* define SIOCGIFADDR */
#else
#include <linux/sockios.h>
#endif

extern char *      my_MCAST_ADDR;  /* defined in multicaster.c */
extern int         my_PORT;        /* ditto */
extern int         my_TTL;
extern int         my_LOOP;
extern char *      my_IFname;      /* defined in multicaster.c */
extern int         verbose;

extern unsigned int nPages;
extern unsigned int last_bytes;    /* the number of bytes in the last page of a file */
extern int cur_entry;
extern char* cmd_name[];

/* Where have we last sent from? */
int                most_recent_file;
int                most_recent_fd;

/* 
   Send buffer for storing the data and to be transmitted thru UDP 
   The format:
   (5*sizeof(int) bytes header) + (PAGE_SIZE data area)

   The header has five int_type (4 bytes) int's.
   (1) mode -- for master to give instructions to the target machines.
   (2) current file index (starting with 1)
   (3) current page index (starting with 1) -- gee!
   (4) bytes to be sent in this page via UPD 
   (5) total number of pages for this file

   data_ptr points to the data area.
*/
int               *mode_ptr; /* char type would cause alignment problem on Sparc */
int               *current_file_ptr;
int               *current_page_ptr;
int               *bytes_sent_ptr;
int               *total_pages_ptr;
char              *data_ptr;
char              *fill_here;
char               send_buff[PAGE_BUFFSIZE];

/* Send socket */
int                send_fd;
#ifndef IPV6
struct sockaddr_in send_addr;
#else
struct sockaddr_in6 send_addr;
#endif

/* for final statistics */
off_t real_total_bytes;  
unsigned int real_total_pages;

/*
  set_mode sets the caster into a new mode.
  modes are defined in main.h:
*/
void set_mode(int new_mode)
{
  /* 20060323 convert it to network byte order */
  *mode_ptr = htonl(new_mode);
}

/* init_sends initializes the send buffer */
void init_sends()
{
  most_recent_file = most_recent_fd = -99999;
  real_total_bytes = 0;
  real_total_pages = 0;

  /* pointers for send buffer */
  mode_ptr = (int *)send_buff;  
  current_file_ptr = (int *)(mode_ptr+1);
  current_page_ptr = (int *)(current_file_ptr + 1);
  bytes_sent_ptr   = (int *)(current_page_ptr + 1);
  total_pages_ptr  = (int *)(bytes_sent_ptr + 1);
  data_ptr = (char *)(total_pages_ptr + 1);
  fill_here = data_ptr;

  /* send socket */
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
}

void clear_send_buf()
{
  fill_here = data_ptr;
}

/*
  put file contents into send (UDP) buffer
  return the number of bytes put into the buffer.
*/
ssize_t pack_page_for_file(int page)
{
  if(verbose>=2)
    fprintf(stderr, "Sending page %d of file %d\n", page, current_entry());

  /*** 
     Adjust the position for reading
     NOTE:if the type (off_t) is not given, the large file operation
          would fail.
  ***/
  lseek(most_recent_fd, (off_t)PAGE_SIZE * (off_t)(page - 1), SEEK_SET);

  /* read it and put the content into send_buff */
  /* the max number this read() will return is PAGE_SIZE */
  return read(most_recent_fd, data_ptr, PAGE_SIZE);
}

int fexist(int entry) 
{
  if (entry != most_recent_file) {
    if (most_recent_fd > 0) close(most_recent_fd); /* make sure we close it */

    if((most_recent_fd = open(getFullname(), O_RDONLY, 0)) < 0){
      perror(getFullname());
    }
    most_recent_file = entry;    
  } 
  return (most_recent_fd >= 0);  /* <0 means FAIL */
}


/*
  send_buffer will send the buffer with the file information
  out to the socket connection with the catcher.
  return 0 -- ok, -1 sent failed.
*/
int send_buffer(int bytes_read)
{
    /* send the data */
    if(sendto(send_fd, send_buff, bytes_read + HEAD_SIZE, 
	      0, (const struct sockaddr *)&send_addr, sizeof(send_addr)) < 0) {
      perror("Sending packet");
      return FAIL;
    }
    return SUCCESS;
}

/*
  send_page takes a page from the current file and controls
  sending it out the socket to the catcher.  It calls send_buffer
  to do the actuall call to sendto.
*/
int send_page(int page)
{
  unsigned int bytes;

  if (verbose>=2) fprintf(stderr, "In send_page()\n");
  
  bytes = (unsigned int) pack_page_for_file(page);

  if ((page < nPages && bytes != PAGE_SIZE) || 
      (nPages == page && bytes != last_bytes)) {
    fprintf(stderr, "Critical: read() error, expected_bytes= %d, real= %d, "
	    "page = %d, nPages = %d, for file %s\n",
	    (page<nPages) ? PAGE_SIZE : last_bytes, 
	    bytes, page, nPages, getFullname());
    my_exit(BAD_EXIT);
  }

  /* fill in the header data */
  /* 20060323 -- add htonl so that the codes can work across
     different machines with either little- or big-endian.
     So, before we send out ints, we convert them to network-byte order*/
  *total_pages_ptr = htonl(nPages);
  *bytes_sent_ptr  = htonl(bytes);
  *current_file_ptr = htonl(cur_entry);
  *current_page_ptr = htonl(page);

  /* for final statistics */
  ++real_total_pages;
  real_total_bytes += bytes; 

  if (verbose>=3)
    fprintf(stderr, "Sending page=%d of %d in file %d of %d\n",
	    page, nPages, 
	    cur_entry, total_entries());
  return send_buffer(bytes);
}

/*
  send_test zeroes out the buffers going to the catcher
  and thereby sends a test packet to the catcher.
*/
void send_test()
{
  *mode_ptr = htonl(TEST); /* --> network byte order */
  
  *current_file_ptr = 0;  
  *current_page_ptr = 0; 
  *total_pages_ptr = 0;
  *bytes_sent_ptr = 0;
  
  send_buffer(0);
  fprintf(stderr, "Test packet is sent.\n");
}

void send_cmd(int code, int machine_id) 
{
  /*
    Except for OPEN_FILE_CMD,
     only the header area in the send_buff gets filled
     with data.
  */
  *mode_ptr = htonl(code); /* --> network byte order */    
  *current_page_ptr = htonl(machine_id); /* -1 being all machines */
  *current_file_ptr = htonl(cur_entry);
  *total_pages_ptr = htonl(nPages);

  *bytes_sent_ptr = (code == OPEN_FILE_CMD) ?  htonl(fill_here - data_ptr) :0;

  /* do the header in send_buf */
  send_buffer((code == OPEN_FILE_CMD) ? fill_here - data_ptr : 0);

  /* print message */
  if (verbose>=2) {
    fprintf(stderr, "cmd [%s] sent\n", cmd_name[code]);
  }
}

void pack_open_file_info()
{
  /* prepare udp send_buffer for file info
     (header) (stat_ascii)\0(filename)\0(if_is_link linktar_path)\0
  */
  clear_send_buf();
  fill_here += fill_in_stat(data_ptr); 
  fill_here += fill_in_filename(fill_here);  
  if (is_softlink() || is_hardlink()) {
    fill_here += fill_in_linktar(fill_here);
  }
}

void send_all_done_cmd()
{
  *mode_ptr = htonl(ALL_DONE_CMD); /* --> network byte order */
  
  *current_file_ptr = 0;  
  *current_page_ptr = 0; 
  *total_pages_ptr = 0;
  *bytes_sent_ptr = 0;
  
  send_buffer(0);
  fprintf(stderr, "(ALL DONE)\n");
}

void my_exit(int good_or_bad)
{
  if (send_fd) send_all_done_cmd();
  exit(good_or_bad);
}
