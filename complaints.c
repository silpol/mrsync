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
/*#include "paths.h"*/
#include <sys/types.h>
#include <time.h>
#include <setjmp.h>
#include <signal.h>

extern int          monitorID;   /* defined in multicaster.c */
extern int          my_FLOW_PORT;
extern int          verbose;
extern int          nPages;
extern char *       cmd_name[];
extern unsigned int total_pages, real_total_pages;
extern off_t total_bytes, real_total_bytes;

/* buffer for receiving complaints */
char                flow_buff[FLOW_BUFFSIZE];
int                *code_ptr;  /* What's wrong? */
int                *mid_ptr;   /* machine id */
int                *page_ptr;  /* Which page */

/* receive socket */
int                 complaint_fd;
#ifndef IPV6
struct sockaddr_in  complaint_addr;
#else
struct sockaddr_in6  complaint_addr;
#endif

/* status */
char *missing_page_flag=NULL; /* arrary of size nPages -- dep on the files */
int  *total_missing_page;     /* array of size nMachines -- persistent thru life of program*/
int  *sick_count;
char *bad_machines;           /* array of size nMachines -- persistent thru life of program*/
char *machine_status;         /* array of size nMachines  for ack */
int   nMachines;
int  has_missing;             /* some machines have missing pages for this file (a flag)*/

/* flow control */
int my_ACK_WAIT_TIMES = ACK_WAIT_TIMES;

/*
  init_complaints initializes our buffers to receive complaint information
  from the catchers
*/
void init_complaints() 
{ 
  if (verbose>=2)
    fprintf(stderr, "in init_complaints with FLOW_BUFFSIZE = %d\n", FLOW_BUFFSIZE);
 
  /* get pointers set to the right place in buffer */
  code_ptr = (int *) flow_buff;
  mid_ptr  = (int *) (code_ptr + 1);
  page_ptr = (int *) (mid_ptr + 1);

  /*  Receive socket (the default buffer size is 65535 bytes */
  if (verbose>=2) printf("set up receive socket for complaints\n");
  complaint_fd = rec_socket(&complaint_addr, my_FLOW_PORT);

  
  /* 
     getsockopt(complaint_fd, SOL_SOCKET, SO_RCVBUF, &i, &il);
     printf(" rcvbuf = %d  type = %d\n", i, il);
     exit(0);
  the default in our machines -> size = 65535 and type = 4 
  Since our flow_buff size is only 6 bytes,
  65535 bytes buffer can handel more than 10K pages!
  */
}

void init_missing_page_flag(int n)
{
  int i;
  nPages = n; 
  if ((missing_page_flag = malloc(n * sizeof(char)))==NULL) {
    fprintf(stderr, "Cannot malloc(%d * sizeof(char))\n", n);
    perror("error = ");
    exit(0);
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

void set_has_missing()
{
  has_missing = TRUE;
}

void reset_has_missing()
{
  has_missing = FALSE;
}

int  has_missing_pages()
{ /* originally, this fx use missing_page_flag[] 
     That will not work if the master did not receive missing page info */
  return has_missing;
}

void refresh_machine_status()
{
  int i;
  for(i=0; i<nMachines; ++i) machine_status[i] = NOT_READY;
}

/* 20040708 for the loop in wait_for_eof 
   During the second and third resending data, the machine status
   can be (OK_MISSING_PAGES, OK, NOT_READY).
   OK_MISSING_PAGES -> the machine has sent back ack, but it has missing pages.
   This refresh is to change OK_MISSING_PAGES to NOT_READY.
   20060421 redundant, only confuses the logic
void refresh_machine_status1()
{
  int i;
  for(i=0; i<nMachines; ++i) {
    if (machine_status[i] == MACHINE_OK) continue;
    machine_status[i] = NOT_READY;
  }
}
*/

void init_machine_status(int n)
{
  int i;
  nMachines = n;
  machine_status = malloc(n * sizeof(char));
  refresh_machine_status();

  bad_machines = malloc(n * sizeof(char));
  for(i=0; i<nMachines; ++i) {
    bad_machines[i] = GOOD_MACHINE;
  }

  total_missing_page = malloc(n * sizeof(int));
  for(i=0; i<nMachines; ++i) {
    total_missing_page[i] = 0;
  }

  sick_count =  malloc(n * sizeof(int));
  for(i=0; i<nMachines; ++i) {
    sick_count[i] = 0;
  }
}

int get_total_missing_pages(int n)
{
  return total_missing_page[n];
}

int read_handle_complaint()
{
  /* 
     return 1 for receiving complaint
     return 0 for no complaint handled 
  */
  int mid_v, code_v, page_v, bytes_read;

  if (readable(complaint_fd)) {
    /* There is a complaint */
    bytes_read = recvfrom(complaint_fd, flow_buff, FLOW_BUFFSIZE, 0, NULL, NULL);

    /* 20060323 deal with big- vs little-endian issue
       convert incoming integers into host representation */

    mid_v = ntohl(*mid_ptr);
                
    if ((bytes_read != FLOW_BUFFSIZE) ||
	(mid_v >= (unsigned int) nMachines)  || /* for safety */ 
	(bad_machines[mid_v] == BAD_MACHINE)) { /* ignore complaint from a bad machine*/	
      return 0; 
    }

    code_v = ntohl(*code_ptr);
    page_v = ntohl(*page_ptr);

    switch (code_v) {
    case PAGE_RECV:
      /********  check if machineID is the one we have set. */
      if (verbose>=2) printf("mid_ptr-> %d, monitorid = %d\n", mid_v, monitorID);
      if (mid_v == monitorID)
	return 1;
      else
	return 0;
    case TOO_FAST :
      /* 
	 Since I turned off the check_queue() in read_handle_page()
	 in page_reader.c,
	 the master will never receive TOO_FAST complaints as it is.
	 This part may be useful in the future for better timing control.
      */
      /* 
	 If the complaint is that we are sending data too fast AND
	 we haven't heeded a request recently 
      */                  
      if (verbose>=2) fprintf(stderr, "*");

      /* Pause for complainer to empty buffer */
      usleep(USEC_TO_IDLE);
      return 1;
    case MONITOR_OK:
      /*********  check if machineID is the one we have set. */
      if (mid_v == monitorID)
	return 1;
      else
	return 0;
    case OPEN_OK :
    case CLOSE_OK :
    case EOF_OK :
      machine_status[mid_v] = MACHINE_OK;
      return 1;
    case MISSING_PAGE :
      if (page_v > nPages) return 1;  /* *page_ptr = page # (1 origin) */
      ++(total_missing_page[mid_v]);
      missing_page_flag[page_v-1] = MISSING; /* the packet may not arrive at master */     
      return 1;
    case MISSING_TOTAL:
      if (page_v > nPages) return 1;
      set_has_missing();                     /* store the info about missing info */
      machine_status[mid_v] = MACHINE_OK;    /* machine_status serves as ack only */
      return 1;
    case SIT_OUT :
      ++(sick_count[mid_v]);
      if (verbose>=1) fprintf(stderr, "machine[%d] sits out receiving this file.\n", mid_v);
      machine_status[mid_v] = MACHINE_OK;
      return 1;
    default :
      if (verbose>=2) fprintf(stderr, "Unknown complaint: code = %d\n", code_v);
      return 0;
    } /* end of switch */
  } /* end of if(readable) */

  /* time out of readable() */
  return 0;
}

int all_machine_ok()
{
  int i;
  for(i=0; i<nMachines; ++i) {
    if (machine_status[i] == NOT_READY && bad_machines[i] == GOOD_MACHINE) 
      return FALSE; /* there is at least one machine that has not sent ack */
  }
  return TRUE;     /* all machines are ready */
}

/* this is for the master to receive the acknowledgement. */
void wait_for_ok(int code)
{
  int i, count;
  time_t tloc;
  time_t rtime0, rtime1; 
  
  rtime0 = time(&tloc);  /* reference time */
 
  count = 0;
  while (!all_machine_ok()) {
    if (read_handle_complaint()==1) { /* if there is a complaint handled */
      rtime0 = time(&tloc);          /* reset the reference time */
      continue;
    }
    /* no complaints handled within the time period set by set_delay() */
    rtime1 = time(&tloc);    /* time since last complaints */
    if ((rtime1-rtime0) >= ACK_WAIT_PERIOD) {
      ++count;
      if (count < my_ACK_WAIT_TIMES) {
	if (verbose>=1 && (count % 10 == 0))
	  fprintf(stderr, "   %d: resend cmd(%s) to machines:[ ", count, cmd_name[code]);
	for(i=0; i<nMachines; ++i) {
	  if (machine_status[i] == NOT_READY && bad_machines[i] == GOOD_MACHINE) {
	    if (verbose>=1 && (count % 10 == 0)) fprintf(stderr, "%d ", i);
	    send_cmd(code, (int) i);
	    usleep(FAST);
	  }
	}
	if (verbose>=1 && (count % 10 == 0)) fprintf(stderr, "]\n");
	rtime0 = rtime1;
      } else { /* allowable period of time has passed */
	if (verbose>=1) fprintf(stderr, "   Drop these bad machines:[ ");
	for(i=0; i<nMachines; ++i) {
	  if (machine_status[i] == NOT_READY && bad_machines[i] == GOOD_MACHINE) {
	    if (verbose>=1) fprintf(stderr, "%d ", i);
	    bad_machines[i]= BAD_MACHINE;
	    /* kill_machine(i); */ 
	    /* 
	       As it is, the signal(SIGALRM) in kill_machine() 
	       fails the second time round.
	       But we really don't need to kill the job.
	       The pages send by the master will be ignored by
	       the bad machines anyway, because the current_file nubmer 
	       does not match.
	    */
	  }
	}
	if (verbose>=1) fprintf(stderr, "]\n");
	break;
      }
    }
  }
}

int  is_it_missing(int page)
{
  return (missing_page_flag[page]==MISSING) ? TRUE : FALSE;
}

int pr_missing_pages()
{
  int i, N, exit_code=0;
  off_t delta;
  unsigned int dp;

  for(i=0; i<nMachines; ++i) {
    char name[PATH_MAX];
    N = get_total_missing_pages(i);
    strcpy(name, id2name(i));
    if (strlen(name)==0) sprintf(name, "machine(%3d)", i);
    fprintf(stderr, "%s: #_missing_page_request = %6.2f%% = %d\n", 
	    name, (double)N/((double)total_pages)*100.0, N);
  }

  for(i=0; i<nMachines; ++i) {
    char name[PATH_MAX];
    if (sick_count[i] == 0) continue;
    strcpy(name, id2name(i));
    if (strlen(name)==0) sprintf(name, "machine(%3d)", i);    
    fprintf(stderr, "%s had skipped %d files.\n", 
	    name, sick_count[i]);
    exit_code=-1;
  }
 
  fprintf(stderr, "\ntotal number of files = %d\n", total_entries());
  dp = real_total_pages - total_pages;
  fprintf(stderr, "Total number of pages = %12d Pages re-sent = %12u (%6.2f%%)\n", 
	  total_pages, dp, (double)dp/(double)total_pages*100.0);
  delta = (off_t)(real_total_bytes - total_bytes);

  #ifdef _LARGEFILE_SOURCE
  fprintf(stderr, "Total number of bytes = %12llu Bytes re-sent = %12llu (%6.2f%%)\n",
	  total_bytes, delta, (double)delta/(double)total_bytes*100.0);
  #else
  fprintf(stderr, "Total number of bytes = %12d Bytes re-sent = %12u (%6.2f%%)\n",
	  total_bytes, delta, (double)delta/(double)total_bytes*100.0);
  #endif

  return (exit_code);
}  

/* count the number of bad machines */
int nBadMachines()
{ 
  int i, count = 0; 
  for(i=0; i<nMachines; ++i) {
    if (bad_machines[i] == BAD_MACHINE) ++count;
  }
  
  return count;
}

int choose_print_machines(char *stArray, char selection, char * msg_prefix)
{
  int i, count = 0;

  for(i=0; i<nMachines; ++i) {
    char line[PATH_MAX];
    if (stArray[i] == selection) {
      ++count;
      strcpy(line, id2name(i));
      if (count == 1) { fprintf(stderr, msg_prefix); }
      if (strlen(line)==0)
	fprintf(stderr, "%d ", i);
      else
	fprintf(stderr, "%s ", line);
    }
  }
  if (count > 0) fprintf(stderr, "]\n");
  return count;
}

int send_done_and_pr_msgs(double total_time)
{
  int exit_code =0;
  send_all_done_cmd();
  
  exit_code = pr_missing_pages();

  fprintf(stderr, "Total time spent = %6.2f (min) ~ %6.2f (min/GB)\n\n", 
	  total_time, total_time / ((double)real_total_bytes/1.0e9));

  exit_code = choose_print_machines(bad_machines, 
				    BAD_MACHINE, 
				    "Not synced for bad machines:[ ");

  if (current_entry() < total_entries()) { /* if we exit prematuredly */
    exit_code = choose_print_machines(machine_status, 
				      NOT_READY, "\nNot-ready machines:[  ");
  }

  if (verbose>=1) pr_rtt_hist();
  return (exit_code);
}

/* to do some cleanup before exit IF all machines are bad */
void do_badMachines_exit()
{
  if (nBadMachines() < nMachines) return;
  fprintf(stderr, "All machines are bad. Exit!\n");
  send_done_and_pr_msgs(-1.0);
  exit(-1);
}


void do_cntl_c(int signo) 
{  
  fprintf(stderr, "Control_C interrupt detected!\n");

  send_done_and_pr_msgs(-1.0);  
  exit(-1);
}
