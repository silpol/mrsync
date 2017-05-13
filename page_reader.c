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

/* the following is needed on Sun but not on linux */
#ifdef _SUN
#include <sys/filio.h>
#endif

extern int machineID;
extern int verbose;
extern char * my_MCAST_ADDR;     /* defined in multicatcher.c */
extern char * my_IFname;
extern int    my_PORT;
extern unsigned int current_file_id;

int          isMonitor;          /* flag = if this target machine is a designated monitor */
int          nPage_recv;         /* counter for the number of pages received for a file */
int          machineState;       /* there are four states during one file transmission */
int          isFirstPage=TRUE;   /* flag */
     
/* The followings are  used to determine sick condition */
int          current_missing_pages; 
int          last_missing_pages;
int          sick_count; 

/* receive socket */
int                     recfd;
#ifndef IPV6
struct sockaddr_in	rec_addr;
#else
struct sockaddr_in6	rec_addr;
#endif

/* 
   Receive buffer for storing the data obtained from UDP 
   The format:
   (5*sizeof(int) bytes header) + (PAGE_SIZE data area)

   The header has five int_type (4 bytes) int's.
   (1) mode -- for master to give instructions to the target machines.
   (2) current file index (starting with 1)
   (3) current page index (starting with 1)
   (4) bytes that has been sent in this UDP page
   (5) total number of pages.

   data_ptr points to the data area.
*/
int                     *mode_ptr;    /* hp: change from char to int */
int                     *total_pages_ptr;
int                     *current_page_ptr;
int                     *bytes_sent_ptr, *current_file_ptr;
char                    *data_ptr;
char                    rec_buf[PAGE_BUFFSIZE];

void init_page_reader()
{
  /*struct ip_mreq mreq;*/
  int rcv_size;

  machineState = IDLE_STATE;
  isMonitor    = FALSE;

  /* Prepare buffer pointers */
  mode_ptr         = (int*)rec_buf;  
  current_file_ptr = (int *)(mode_ptr + 1);
  current_page_ptr = (int *)(current_file_ptr + 1);
  bytes_sent_ptr   = (int *)(current_page_ptr + 1);
  total_pages_ptr  = (int *)(bytes_sent_ptr + 1);
  data_ptr         = (char *)(total_pages_ptr + 1);

  /* Set up receive socket */
  if (verbose>=2) fprintf(stderr, "setting up receive socket\n");
  recfd = rec_socket(&rec_addr, my_PORT);

  /* Join the multicast group */
  /*inet_pton(AF_INET, MCAST_ADDR, &(mreq.imr_multiaddr.s_addr)); 
  mreq.imr_multiaddr.s_addr = inet_addr(my_MCAST_ADDR);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY);  
  
  if (setsockopt(recfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0){
    perror("Joining Multicast Group");
  }
  */

  if (Mcast_join(recfd, my_MCAST_ADDR, my_IFname, 0)<0) {
    perror("Joining Multicast Group");
  }

  /* Increase socket receive buffer */
  rcv_size = TOTAL_REC_PAGE * PAGE_BUFFSIZE;
  if (setsockopt(recfd, SOL_SOCKET, SO_RCVBUF, &rcv_size, sizeof(rcv_size)) < 0){
    perror("Expanding receive buffer for page_reader");
  }

}

/*
  This is the heart of multicatcher.
  It parses the incoming pages and do proper reactions according
  to the mode (command code) encoded in the first 4 bytes in an UDP page.
  It returns the mode.
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
  int mode_v, bytes_sent_v, total_pages_v, current_file_v, current_page_v;

  /* -----------receiving data -----------------*/
  if (readable(recfd) == 1) { /* there is data coming in */
    /* check_queue(); This might be useful. But need more study. */
    /* get data */
    bytes_read = recvfrom(recfd, rec_buf, PAGE_BUFFSIZE, 0, 
			  (struct sockaddr *)&return_addr, 
			  (socklen_t*) &return_len);

    bytes_sent_v = ntohl(*bytes_sent_ptr);
    if (bytes_read != (bytes_sent_v + HEAD_SIZE))
      return NULL_CMD;

    /* convert from network byte order to host byte order */ 
    mode_v = ntohl(*mode_ptr);    
    total_pages_v = ntohl(*total_pages_ptr);
    current_file_v = ntohl(*current_file_ptr);
    current_page_v = ntohl(*current_page_ptr);

    if (isFirstPage) {
      update_complaint_address(&return_addr);
      isFirstPage = FALSE;
    }
   
    /* --- process various commands (modes) */
    switch (mode_v) {
    case TEST:
      /* It is just a test packet? */
      fprintf(stderr, "********** Received test packet **********\n");
      return mode_v;

    case SELECT_MONITOR_CMD:
      if (current_page_v == machineID) {
	isMonitor = TRUE;
	send_complaint(MONITOR_OK, machineID, 0, 0);
      } else {
	isMonitor = FALSE;
      }      
      return mode_v;

    case OPEN_FILE_CMD:      
      if ((current_page_v == (int) ALL_MACHINES || current_page_v == (int) machineID)
	  && machineState == IDLE_STATE) {
	/* get info about this file */
	if (verbose>=1)
	  fprintf(stderr, "open file id= %d\n", current_file_v);
	if (!extract_file_info(data_ptr, current_file_v, total_pages_v))
	  return mode_v;

	/* different tasks here */
	/* open file, rmdir, unlink */
	if (total_pages_v < 0) { /* delete a file or a directory */
	  if (!delete_file()) return mode_v; /* this fx can be re-entered many times */	  
	  /* machineState remains to be IDLE_STATE */
	} else if (total_pages_v == 0) { 
	  /* handle an empty file, or (soft)link or directory */
	  if (!check_zero_page_entry()) return mode_v; /* can re-enter many times */ 
	  /* machineState remains to be IDLE_STATE */
	} else { /* a regular file */
	  if (!open_file()) return mode_v;
	  /* the file has been opened. */
	  sick_count = 0;
	  current_missing_pages =0;
	  last_missing_pages = nPages_for_file(current_file_v);
	  machineState = GET_DATA_STATE;
	}
	send_complaint(OPEN_OK, machineID, 0, current_file_id); /* ack */
	nPage_recv = 0;
	return mode_v;
      }
      /* 
	 We must be in GET_DATA_STATE.
	 OPEN_OK ack has been sent back in the previous block.
         However,
	 the master may not have received the ack.
         In that case the master will send back the open_file_cmd again.
      */
      if ((current_page_v == (int) ALL_MACHINES ||current_page_v == (int) machineID)
	  && (machineState == GET_DATA_STATE)) { /****/
	send_complaint(OPEN_OK, machineID, 0, current_file_id); 
      }
      return mode_v;

    case EOF_CMD:
      /**********/
      if (verbose>=1)
	fprintf(stderr, "***** EOF received for id=%d state=%d id=%d, file=%d\n", 
		current_page_v, machineState, machineID, current_file_v);

      /* the following happnens when this machine was previously out-of-pace
         and was labeled as 'BAD MACHINE' by the master.
         The master has proceeded with the syncing process without
         waiting for this machine to finish the process in one of the previous files.
         Since under normal condition, this machine should not expect to see
         current_file changes except when OPEN_FILE_CMD is received.
      */
      if ((current_file_v) != current_file_id) return mode_v;  /* ignore the cmd */

      /* normal condition */
      if ((current_page_v == (int) ALL_MACHINES || current_page_v == (int) machineID)
	  && machineState == GET_DATA_STATE) { /* GET_DATA_STATE */
	/* check missing pages and send back missing-page-request */
	current_missing_pages = ask_for_missing_page();

	if (current_page_v == (int) machineID) {
	  /* master is asking for my EOF_ack */
	  if (current_missing_pages == 0) { 
	    /* w/o assuming how we get to this point ... */
	    machineState = DATA_READY_STATE;
	    send_complaint(EOF_OK, machineID, 0, current_file_id);
	  } else {	  
	    send_complaint(MISSING_TOTAL, machineID, 
			   current_missing_pages , current_file_id);
	  }
	  return mode_v;
	}

	/***
	   master is asking everyone, (after master sent or re-sent pages) 
	   so, we do some book-keeping procedures (incl state change)
	***/
	if (verbose >=1)
	  fprintf(stderr, "missing_pages = %d, nPages_received = %d file = %d\n",
		  current_missing_pages, nPage_recv, current_file_v); /************/	
	
	nPage_recv = 0;

	if (current_missing_pages == 0) {
	  /* 
	     There is no missing page.
	     Change the state.
	  */
	  machineState = DATA_READY_STATE;
	  send_complaint(EOF_OK, machineID, 0, current_file_id);
	  return mode_v;
	} else {
	  /* 
	     There are missing pages.
	     If we still miss many pages for SICK_THRESHOLD consecutive times,
	     then we are sick. e.g. machine CPU does not give multicatcher
             enough time to process incoming UDP's OR the disk I/O is too slow.
	  */

	  if ((SICK_RATIO)*(double)last_missing_pages < (double)current_missing_pages) {
	    ++sick_count;
	    if (sick_count > SICK_THRESHOLD) {
	      machineState = SICK_STATE;
	      send_complaint(SIT_OUT, machineID, 
			     0, current_file_id); /* no more attempt to receive */
	      /* master may send more pages from requests from other machines
		 but this machine will mark this file as 'sits out receiving' */
	    } else {
	      /* not sick enough yet */
	      send_complaint(MISSING_TOTAL, machineID, 
			     current_missing_pages, current_file_id);
	      /* master will send more pages */
	    }
	  } else { /* we are getting enough missing pages this time to keep up */
	    sick_count = 0; /* break the consecutiveness */
	    send_complaint(MISSING_TOTAL, machineID, 
			   current_missing_pages, current_file_id);
	    /* master will send more pages */
	  }
	  last_missing_pages = current_missing_pages;	  
	  return mode_v;
	} 
      } /* end GET_DATA_STATE */

      /* After state change, we still get request for ack.
	 send back ack again */
      if ((current_page_v == (int) ALL_MACHINES || current_page_v == (int) machineID)) {
	switch (machineState) {
	case DATA_READY_STATE:
	  send_complaint(EOF_OK, machineID, 
			 0, current_file_id);
	  return mode_v;
	case SICK_STATE:
	  send_complaint(SIT_OUT, machineID, 
			 0, current_file_id);	/* just an ack, even for sick state*/
	  return mode_v;
	}
      }    
      return mode_v;

    case CLOSE_FILE_CMD:
      if (verbose>=1)
	fprintf(stderr, "***** CLOSE received for id=%d state=%d id=%d, file=%d\n", 
		current_page_v, machineState, machineID, current_file_v);

      if ((current_file_v) != current_file_id) return mode_v;  /* ignore the cmd */

      if (current_page_v == (int) ALL_MACHINES || current_page_v == (int) machineID) {
	if (machineState == DATA_READY_STATE) {
	  if (!close_file()) { return mode_v; };
	  set_owner_perm_times();
	  machineState = IDLE_STATE;
	  send_complaint(CLOSE_OK, machineID, 0, current_file_id);
	  return mode_v;
	} else if (machineState == IDLE_STATE) {
	  /* send ack back again because we are asked */	  
	  send_complaint(CLOSE_OK, machineID, 0, current_file_id);	  
	  return mode_v;
	} else { /* other states -- we should not be here*/
	  /* if (machineState == SICK_STATE || machineState == GET_DATA_STATE) */
	  /* SICK_STATE --> we are too slow in getting missing pages
                 if one of the machines is sick, master will send out CLOSE_ABORT
	     GET_DATA_STATE -->
	        We are not supposed to be in GET_DATA_STATE, 
	        so consider it a sick_state */
	  fprintf(stderr, "*** should not be here -- state=%d\n", machineState);
	  if (!rm_tmp_file()) { return mode_v; };
	  machineState = IDLE_STATE;
	  send_complaint(SIT_OUT, machineID, 0, current_file_id);	 
	  /* make sick_count larger than threshold for GET_DATA_STATE */
	  sick_count = SICK_THRESHOLD + 10000;
	  return mode_v;
	}
      }
      return mode_v;

    case CLOSE_ABORT_CMD:
      if (verbose>=1)
	fprintf(stderr, "***** CLOSE_ABORT received for id=%d state=%d id=%d, file=%d\n", 
		current_page_v, machineState, machineID, current_file_v);

      if ((current_file_v) != current_file_id) return mode_v;  /* ignore the cmd */

      if (current_page_v == (int) ALL_MACHINES || current_page_v == (int) machineID) {	
	if (!rm_tmp_file()) { return mode_v; };
	machineState = IDLE_STATE;
	send_complaint(CLOSE_OK, machineID, 0, current_file_id);
      }
      return mode_v;

    case SENDING_DATA:
    case RESENDING_DATA:
      if ((current_file_v) != current_file_id) return mode_v;  /* ignore the cmd */
      /* 
	 otherwise, go ahead...
      */
      if (verbose>=2) {
	fprintf(stderr, "Got %d bytes from page %d of %d for file %d mode=%d\n", 
		bytes_read - HEAD_SIZE,
		current_page_v, total_pages_v, 
		current_file_v + 1, mode_v); 
      }

      /* timing the disk IO */
      /* start_timer(); */

      write_page(current_page_v, data_ptr, bytes_read - HEAD_SIZE);
      if (isMonitor) send_complaint(PAGE_RECV, machineID, 0, current_file_id);

      /*
      end_timer();
      update_time_accumulator();
      */
      
      /* Yes, we have just read a page */
      ++nPage_recv; 
      return mode_v;

    case ALL_DONE_CMD:
      /* 
	 clear up the files.
      */
      /*** since we do not know if there are other machines
           that are NOT in data_ready_state,
           to maintain equality, it is best to just
           remove tmp_file without close_file() ***/      
      rm_tmp_file();
      return mode_v;

    default:
      return mode_v;
    } /* end of switch */
  } else {
    /* No,  the read is timed out */
    return TIMED_OUT;
  }
}

