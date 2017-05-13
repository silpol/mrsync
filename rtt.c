/*
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>  
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
  
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

/*  
   To measure round trip time (RTT) using UDP
*/

#include "rttmain.h"
#include <limits.h> /* to define PATH_MAX */
#include <libgen.h>

char * my_MCAST_ADDR = MCAST_ADDR;
int  my_FLOW_PORT    = FLOW_PORT;
int  my_PORT         = PORT;
int  my_TTL          = MCAST_TTL;
int  my_LOOP         = MCAST_LOOP;
char * my_IFname     = MCAST_IF;

int    no_feedback_count;
char * machine           = NULL;
int    remote_pid;
char * reshell       = REMOTE_SHELL;
char catcher_path[PATH_MAX];

void usage()
{
  fprintf(stderr, 
	  "rtt (to measure rount trip time to a target version %s\n"
	  "   Option list:\n"
	  "   [ -v flag to turn on verbose]\n"
	  "   [ -r <remote shell, default=%s> ]\n"
	  "   [ -p <PATH for remote rttcatcher, default=%s> ]\n"
	  "   -------- essential options ----------------------------------\n"
	  "     -m <destination machine_name>\n" 
	  "     -n <num Of Pages>\n"
	  "     -s <page_size in bytes; max=64512>\n"
	  "   -------- mcast options --------------------------------------\n"
	  "   [ -A <my_mcast_address default=%s)> ]\n"
	  "   [ -P <my_PORT default=%d> ]\n"
	  "   [ -T <my_TTL default=%d> ]\n"
	  "   [ -L flag turn on mcast_LOOP. default is off ]\n"
	  "   [ -I <my_MCAST_IF default=NULL> ]\n",
	  VERSION, reshell, catcher_path, my_MCAST_ADDR, my_PORT, my_TTL);
}

void get_dirname_of_rtt(char *dname, char *rtt_path)
{
  char path[PATH_MAX];
  strcpy(path, rtt_path); /* dirname() will change its argument */
  strcpy(dname, dirname(path));
  
  if (strcmp(dname, ".")==0) {
    strcpy(dname, getcwd(path, PATH_MAX));
  }
}

void do_one_page(int page)
{
  unsigned long rtt;
  refresh_timer();
  start_timer();
  send_page(page);
  /* read_handle_complaint() waits n*interpage_interval at most */
  if (read_handle_complaint()==0) {  /* delay_sec for readable() is set by set_delay() */
    /* 
       At this point, the readable() returns without getting a reply 
       from the target after n*DT_PERPAGE 
       This indicates that the page has likely been lost in the network. 
    */
    if (verbose) fprintf(stderr, "no ack for page = %d\n", page);
    ++no_feedback_count;   
    update_rtt_hist(999999);  /* register this as rtt = infinite --- the last element in rtt_hist */
    if (no_feedback_count>NO_FEEDBACK_COUNT_MAX) { /* count the consecutive no_feedback event */
      /* switch to another client */
      fprintf(stderr, "Consecutive non_feedback exceeds limit. Continue with next page...\n");
      no_feedback_count = 0;
    }
  } else {
    end_timer();
    update_time_accumulator();
    rtt = get_accumulated_usec();
    /* to do: wait additional time after receiving feedback: usleep( rtt * 0.1 ); */
    /* to do: update histogram */
    if (verbose>=2) printf("rtt(p = %d) = %ld (usec)\n", page, rtt);
    update_rtt_hist(rtt);
    
    no_feedback_count = 0;
  }
}

int invoke_catcher(char * machine)
{
  FILE *ptr;
  char buf[PATH_MAX];

  /* invoke rttcatcher on remote machine */  
  fprintf(stderr, "using %s to invoke rttcatcher on %s\n", reshell, machine);

  /* check if rsh (ssh) works */
  sprintf(buf, "%s %s date", reshell, machine);
  if (verbose) fprintf(stderr, "%s\n", buf);
  if (system(buf)) {
    fprintf(stderr, "cannot do rsh to the target machine = %s\n", machine);
    exit(BAD_EXIT);
  }

  if (!my_IFname) {
    sprintf(buf, 
	    "%s %s '%s/rttcatcher -A %s -P %d 1>/dev/null 2>/dev/null & echo $!'", 
	    reshell, machine, catcher_path, my_MCAST_ADDR, my_PORT);
  } else {
    sprintf(buf, 
	    "%s %s '%s/rttcatcher -A %s -P %d -I %s 1>/dev/null 2>/dev/null & echo $!'", 
	    reshell, machine, catcher_path, my_MCAST_ADDR, my_PORT, my_IFname);
  }
  
  
  fprintf(stderr, "%s\n", buf);
  if ((ptr = popen(buf, "r")) == NULL) {
    fprintf(stderr, "Failure to invoke rttcather\n");
    exit(-1);
  }
  fgets(buf, PATH_MAX, ptr);
  pclose(ptr);

  return atoi(buf);
}

int main(int argc, char *argv[])
{
  char c;
  int nPages =-1, pageSize=-1, ipage;
  
  verbose = 0;
  catcher_path[0] = '\0';

  while ((c = getopt(argc, argv, "vm:s:n:r:p:A:P:T:LI:")) !=  EOF) {
    switch (c) {
    case 'v': 
      verbose = 1;
      break;
    case 'r':
      reshell = optarg;
      break;
    case 'p':
      strcpy(catcher_path, optarg);
      break;
    case 'A':
      my_MCAST_ADDR = optarg;
      break;
    case 'P':
      my_PORT  = atoi(optarg);
      my_FLOW_PORT = my_PORT -1;
      break;  
    case 'T':
      my_TTL = atoi(optarg);
      break;
    case 'L':
      my_LOOP = TRUE;
      break;
    case 'I':
      my_IFname = optarg;
      break;
    case 'n' :
      nPages = atoi(optarg);
      break;
    case 'm':
      machine = optarg;
      break;
    case 's':
      pageSize = atoi(optarg);
      if (pageSize>MAX_PAGE_SIZE) {
	usage();
	exit(-1);
      }
      break;
    case '?':
      usage();
      exit(-1);
    }
  }

  if (strlen(catcher_path)==0) { 
    get_dirname_of_rtt(catcher_path, argv[0]);
  }
  

  if (nPages == -1 || pageSize == -1 || machine == NULL) {
    fprintf(stderr, "Essential options (-n -m -s) should be specified. \n"); 
    usage();
    exit(-1);
  }

  /* init */
  init_sends(pageSize);   
  init_complaints();

  /* set up Cntl_C catcher */
  Signal(SIGINT, do_cntl_c);

  remote_pid = invoke_catcher(machine);
  fprintf(stderr, "remote pid = %d\n", remote_pid);
  
  sleep(1);
  
  /* -------------------Send data-------------------------------------- */
 
  init_missing_page_flag(nPages);
   
  send_cmd(START_CMD, nPages);
  refresh_machine_status();
  wait_for_ok(START_CMD); 
  do_badMachines_exit(machine, remote_pid);

  fprintf(stderr, "Sending data...\n");
  /* send pages */
  set_mode(SENDING_DATA);  
  set_delay(0, DT_PERPAGE*FACTOR); 
  
  no_feedback_count = 0;
  for (ipage = 0; ipage < nPages; ipage++){
    do_one_page(ipage);
  }

  send_cmd(EOF_CMD, 0);
  refresh_machine_status();
  wait_for_ok(EOF_CMD);
  do_badMachines_exit("", -1);

  /* -----------------end of send data -------------------------------- */

  free_missing_page_flag();
  send_done_and_pr_msgs();
  return 0;
}

