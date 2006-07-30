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
int                *file_ptr;  /* which file */
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
int *last_seq;                /* array of size nMachines -- seq number of complaints */
                              /* *** watch out the max seq = 2e9 */
int  *total_missing_page;     /* array of size nMachines -- persistent thru life of program*/
char *file_received;          /* array of size nMachines */
char *bad_machines;           /* array of size nMachines -- persistent thru life of program*/
char *machine_status;         /* array of size nMachines  for ack */
int  *missing_pages;          /* array of size nMachines */

int   nMachines;
int  has_missing;             /* some machines have missing pages for this file (a flag)*/
int  has_sick;                /* some machines are sick for this file (a flag)*/
int  skip_count=0;            /* number of files that are not delivered */
int  quitWithOneBad=FALSE;    /* default: continue with one or more (<all) bad machine */

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
  file_ptr = (int *) (mid_ptr + 1);
  page_ptr = (int *) (file_ptr + 1);

  /*  Receive socket (the default buffer size is 65535 bytes */
  if (verbose>=2) printf("set up receive socket for complaints\n");
  complaint_fd = rec_socket(&complaint_addr, my_FLOW_PORT);
  
  /* 
     getsockopt(complaint_fd, SOL_SOCKET, SO_RCVBUF, &i, &il);
     printf(" rcvbuf = %d  type = %d\n", i, il);
     exit(0);
  the default in our machines -> size = 65535 and type = 4 
  Since our flow_buff size is only sizeof(int)*4 bytes,
  65535 bytes buffer can handle more than 4K pages at a time!
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

void set_has_sick()
{
  has_sick = TRUE;
}

void reset_has_sick()
{
  has_sick = FALSE;
}

int has_sick_machines()
{
  return has_sick;
}

int  has_missing_pages()
{ /* originally, this fx use missing_page_flag[] 
     That will not work if the master did not receive missing page info */
  return has_missing;
}

void refresh_missing_pages()
{
  int i;
  for(i=0; i<nMachines; ++i) missing_pages[i] = 0;
}

void refresh_machine_status()
{
  int i;
  for(i=0; i<nMachines; ++i) machine_status[i] = NOT_READY;
}

void mod_machine_status()
{
  /* take into account of machines which have received this file */
  int i;
  for(i=0; i<nMachines; ++i) {
    if (file_received[i]==FILE_RECV) machine_status[i]=MACHINE_OK;
  }
}

void refresh_file_received()
{
  int i;
  for(i=0; i<nMachines; ++i) file_received[i] = NOT_RECV;
}

int nNotRecv()
{
  int i, c;
  c = 0;
  for(i=0; i<nMachines; ++i) {
    if (file_received[i] == NOT_RECV) ++c;
  }
  return c;
}

int iNotRecv()
{
  /* find the first NotRecv machine */
  int i;
  for(i=0; i<nMachines; ++i) {
    if (file_received[i] == NOT_RECV) return i;
  }
  return -1;
}

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

  missing_pages = malloc(n * sizeof(int));
  for(i=0; i<nMachines; ++i) {
    missing_pages[i] = 0;
  }

  file_received =  malloc(n * sizeof(char));
  refresh_file_received();

  skip_count = 0;
  quitWithOneBad = FALSE;

  last_seq = malloc(n * sizeof(int));
  for(i=0; i<nMachines; ++i) {
    last_seq[i] = -1;
  }
}

int get_total_missing_pages(int n)
{
  return total_missing_page[n];
}

int read_handle_complaint(int cmd)
{
  /* 
     cmd = cmd_code -- each cmd expects different 'response' (complaints)
     return 1 for receiving complaint
     return 0 for no complaint handled 
  */
  int mid_v, code_v, file_v, page_v, bytes_read;

  if (readable(complaint_fd)) {
    /* There is a complaint */
    bytes_read = recvfrom(complaint_fd, flow_buff, FLOW_BUFFSIZE, 0, NULL, NULL);

    /* 20060323 deal with big- vs little-endian issue
       convert incoming integers into host representation */

    mid_v = ntohl(*mid_ptr);
    
            
    if ((bytes_read != FLOW_BUFFSIZE) || (mid_v < 0 ) ||
	(mid_v >= (unsigned int) nMachines)  || /* boundary check for mid_v for safety */
	(bad_machines[mid_v] == BAD_MACHINE)) { /* ignore complaint from a bad machine*/
      return 0; 
    }

    code_v = ntohl(*code_ptr); 
    file_v = ntohl(*file_ptr);
    page_v = ntohl(*page_ptr);

    /* check if the complaint is for the current file */
    if (code_v != MONITOR_OK && file_v != current_entry()) return 0; 
    /* out of seq will be ignored */
    if (code_v != MISSING_PAGE) {
      if (page_v <= last_seq[mid_v]) return 0;
      else last_seq[mid_v] = page_v;
    }

    switch (code_v) {
    case PAGE_RECV:
      /********  check if machineID is the one we have set. */
      if (verbose>=2) fprintf(stderr, "mid_ptr-> %d, monitorid = %d\n", mid_v, monitorID);
      if (cmd == SENDING_DATA && mid_v == monitorID)
	return 1;
      else
	return 0;

    case MONITOR_OK:
      /*********  check if machineID is the one we have set. */
      if (cmd == SELECT_MONITOR_CMD && mid_v == monitorID)
	return 1;
      else
	return 0;

    case OPEN_OK :
      if (cmd == OPEN_FILE_CMD) {
	machine_status[mid_v] = MACHINE_OK;
	return 1;
      } else {
	return 0;
      }

    case CLOSE_OK :
      if (cmd == CLOSE_FILE_CMD || cmd == CLOSE_ABORT_CMD) {
	machine_status[mid_v] = MACHINE_OK;
	return 1;
      } else {
	return 0;
      }

    case EOF_OK :
      if (cmd == EOF_CMD  && file_received[mid_v]==NOT_RECV) {
	machine_status[mid_v] = MACHINE_OK;
	file_received[mid_v] = FILE_RECV;
	return 1;
      } else {
	return 0;
      }

    case MISSING_PAGE :
      if (cmd != EOF_CMD || file_received[mid_v]==FILE_RECV) return 0;
      if (page_v < 1 || page_v > nPages) return 0;  /* page # (1 origin) */
      ++(total_missing_page[mid_v]);
      ++(missing_pages[mid_v]);
      missing_page_flag[page_v-1] = MISSING; /* the packet may not arrive at master */     
      set_has_missing(); 
      return 1;

    case MISSING_TOTAL:
      if (cmd != EOF_CMD || file_received[mid_v]==FILE_RECV) return 0;
      set_has_missing();                     /* store the info about missing info */      
      machine_status[mid_v] = MACHINE_OK;    /* machine_status serves as ack only */      
      return 1;

    case SIT_OUT :
      if (cmd != EOF_CMD || file_received[mid_v]==FILE_RECV) return 0;      
      fprintf(stderr, "*** %s sits-out-receiving %s\n", 
	      id2name(mid_v), getFilename());
      machine_status[mid_v] = MACHINE_OK;

      if (!has_sick) ++skip_count;
      set_has_sick();
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
    if (read_handle_complaint(code)==1) { /* if there is a complaint handled */
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
	fprintf(stderr, "   Drop these bad machines:[ ");
	for(i=0; i<nMachines; ++i) {
	  if (machine_status[i] == NOT_READY && bad_machines[i] == GOOD_MACHINE) {
	    fprintf(stderr, "%d ", i);
	    bad_machines[i]= BAD_MACHINE;
	    /* 
	       The pages sent by the master will be ignored by
	       the bad machine, because the current_file nubmer 
	       does not match.
	    */
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

  if (skip_count>0) {
    fprintf(stderr, "\nWarning: There are %d files which are not delivered.\n", skip_count);
    exit_code = -1;
  }

  fprintf(stderr, "\nTotal number of files = %12d Pages w/o ack = %12u (%6.2f%%)\n", 
	  total_entries(), pages_wo_ack(), (double)pages_wo_ack()/(double)real_total_pages*100.0);

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
  int exit_code1 =0;
  int exit_code2 =0;
  int exit_code3 =0;

  send_all_done_cmd();
  
  exit_code1 = pr_missing_pages();

  fprintf(stderr, "Total time spent = %6.2f (min) ~ %6.2f (min/GB)\n\n", 
	  total_time, total_time / ((double)real_total_bytes/1.0e9));

  exit_code2 = choose_print_machines(bad_machines, 
				     BAD_MACHINE, 
				     "Not synced for bad machines:[ ");

  if (current_entry() < total_entries()) { /* if we exit prematuredly */
    exit_code3 = choose_print_machines(machine_status, 
				       NOT_READY, "\nNot-ready machines:[  ");
  }

  if (verbose>=1) pr_rtt_hist();
  return (exit_code1+exit_code2+exit_code3);
}

/* to do some cleanup before exit IF all machines are bad */
void do_badMachines_exit()
{
  if ((quitWithOneBad && nBadMachines() != 0) ||
      (!quitWithOneBad && (nBadMachines() < nMachines))) return;
  if (quitWithOneBad)
    fprintf(stderr, "One machine is bad. Exit!\n");
  else
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
