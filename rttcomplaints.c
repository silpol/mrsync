/*
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>  
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
                  codes in this file are extracted and modified from complaints.c

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
#include <sys/times.h>

/* buffer for receiving complaints */

char                flow_buff[FLOW_BUFFSIZE];
int                *code_ptr;  /* What's wrong? */  
int                *page_ptr;  /* Which page */

/* receive socket */
int                 complaint_fd;
#ifndef IPV6
struct sockaddr_in  complaint_addr;
#else
struct sockaddr_in6  complaint_addr;
#endif

extern int          my_FLOW_PORT;

/* status */
char *missing_page_flag=NULL;    /* arrary of size nPages -- dep on the files */
int   total_missing_page = 0;    
char  machine_status = NOT_READY;
int   nMachines = 1;
int   nPages;
char *machine_list_file;

extern char * machine;
extern int remote_pid;
extern char * reshell;

/*
  init_complaints initializes our buffers to receive complaint information
  from the catchers
*/
void init_complaints () 
{
  if (verbose)
    fprintf(stderr, "in init_complaints with FLOW_BUFFSIZE = %d\n", FLOW_BUFFSIZE);
 
  /* Buffer */
  code_ptr = (int *)flow_buff;
  page_ptr = (int *)(code_ptr + 1);

  /*  Receive socket */
  if (verbose) printf("set up receive socket for complaints\n");
  complaint_fd = rec_socket(&complaint_addr, my_FLOW_PORT);
}

void init_missing_page_flag(int n)
{
  int i;
  nPages = n; 
  if ((missing_page_flag = malloc(n * sizeof(char)))==NULL) {
    fprintf(stderr, "Cannot malloc(%d * sizeof(char))\n", n);
    perror("error = ");
    exit(-1);
  }
  for(i=0; i<nPages; ++i) {
    missing_page_flag[i] = RECEIVED;
  }
}

void page_sent(int page)
{
  missing_page_flag[page] = RECEIVED;
}

void free_missing_page_flag()
{
  free(missing_page_flag);
  missing_page_flag = NULL;
}


void refresh_machine_status()
{
  machine_status = NOT_READY;
}

int get_total_missing_pages()
{
  return total_missing_page;
}

int read_handle_complaint()
{
  /* 
     return 1 for receiving complaint
     return 0 for no complaint handled 
  */
  int code_v, page_v, bytes_read;

  if (readable(complaint_fd)) {

    /* There is a complaint */
    bytes_read = recvfrom(complaint_fd, flow_buff, FLOW_BUFFSIZE, 0, NULL, NULL);
      
    /* 20060323 deal with big- vs little-endian issue
       convert incoming integers into host representation */
                
    if (bytes_read != FLOW_BUFFSIZE) return 0; 
    
    code_v = ntohl(*code_ptr);
    page_v = ntohl(*page_ptr);      

    switch (code_v) {
    case PAGE_RECV:
      return 1;
    case START_OK :
    case EOF_OK :
      machine_status = MACHINE_OK;
      return 1;
    case MISSING_PAGE :
      if (page_v > nPages) return 1;  /* *page_ptr = page # (1 origin) */
      ++(total_missing_page);
      missing_page_flag[(page_v)-1] = MISSING;
      return 1;
    case LAST_MISSING :
      if (page_v > nPages) return 1;
      ++(total_missing_page);
      missing_page_flag[page_v-1] = MISSING;
      machine_status = MACHINE_OK;
      return 1;
    default :
      printf("Unknown complaint: %d\n", code_v);
      return 0;
    } /* end of switch */
  } /* end of if(readable) */

  return 0;
}

int all_machine_ok()
{
  return (machine_status == NOT_READY ) ? 0 : 1;
}

void wait_for_ok(int code)
{
  int i, count;
  time_t tloc;
  time_t rtime0, rtime1; 
  
  rtime0 = time(&tloc);  /* reference time */
 
  count = 0;
  while (!all_machine_ok()) {
    if (read_handle_complaint()==1) { /* if there is a complaint handled */
      rtime0 = time(&tloc);       /* reset the reference time */
      continue;
    }
    /* no complaints handled */
    rtime1 = time(&tloc);    /* time since last complaints */
    if ((rtime1-rtime0) >= ACK_WAIT_PERIOD) {
      ++count;
      if (count < ACK_WAIT_TIMES) {
	fprintf(stderr, "%d: resend cmd(%d) to machines:[ ", count, code);
	for(i=0; i<nMachines; ++i) {
	  if (machine_status == NOT_READY) {
	    fprintf(stderr, "%d ", i);
	    send_cmd(code, (int) i);
	    usleep(FAST);
	  }
	}
	fprintf(stderr, "]\n");
	rtime0 = rtime1;
      } else {
	fprintf(stderr, "Time out for the 'bad' machines:[ ");
	for(i=0; i<nMachines; ++i) {
	  if (machine_status== NOT_READY) {
	    fprintf(stderr, "%d ", i);
	  }
	}
	fprintf(stderr, "]\n");
	break;
      }
    }
  }
}

int  is_it_missing(int page)
{
  return (missing_page_flag[page]==MISSING) ? 1 : 0;
}

int  has_missing_pages()
{
  int i;
  for(i=0; i<nPages; ++i) {
    if (missing_page_flag[i] == MISSING) {
      return 1;
    }
  }
  return 0;
}

void pr_missing_pages()
{
  int N;
 
  N = get_total_missing_pages();
  fprintf(stderr, "Missing pages = %10d (%5.2f%%) out of total pages = %d\n", 
	  N, (double)N/((double)nPages)*100.0, nPages);
}  

void send_done_and_pr_msgs()
{
  send_all_done_cmd(); 
  pr_missing_pages();  
  pr_rtt_hist();
}

void kill_pid()
{
  char cmd[100];
  /* kill remote process in case where remote machine is not in the multicast network */  
  sprintf(cmd, "%s %s 'kill -9 %d'", reshell, machine, remote_pid);
  fprintf(stderr, "To make sure we clean up the process on remote machine,\n");
  fprintf(stderr, "%s\n", cmd);
  system(cmd);
}

void do_cntl_c(int signo) 
{  
  fprintf(stderr, "Control_C interrupt detected!\n");
  send_done_and_pr_msgs();
  kill_pid();
  exit(-1);
}

/* to do some cleanup before exit IF all machines are bad */
void do_badMachines_exit(char* machine, int pid)
{
  if (machine_status != NOT_READY) return;
  fprintf(stderr, "Remote machine is bad. Exit!\n");
  send_done_and_pr_msgs();

  if (pid > 0) {
    kill_pid();
  }

  exit(-1);
}
