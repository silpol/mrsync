/*
   Copyright (C)  2006 Renaissance Technologies Corp.
             main developer: HP Wei <hp@rentec.com>
                verision 3.0 major update
                  -- large file support
                  -- platform independence (between linux, unix)
                  -- backup feature (as in rsync)
                  -- removing meta-file-info
                  -- catching slow machine as the feedback monitor
		  -- mcast options
                version 3.0.[1-9] bug fixes
                  -- logic flaw which under certain condition
		     caused premature dropout due to 
                     unsuccessful EOF, CLOSE_FILE 
                     and caused unwanranted SIT-OUT cases.
		  -- tested on Debian 64 bit arch by Nicolas Marot in France
	        version 3.1.0
		  -- codes for IPv6 are ready (but not tested)
		     IPv4 is tested ok.

   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
             main developer: HP Wei <hp@rentec.com>
   This file was modified in 2001 from files in the program 
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
#include <stdlib.h>
#include <limits.h> /* to define PATH_MAX */
#include <sys/times.h>
#include <setjmp.h>
#include <signal.h>

extern int    verbose;
extern char * machine_list_file;   /* defined in complaints.c */
extern char * bad_machines;        /* array of size nMachines, defined in complaints.c */
extern int  * total_missing_page; 
extern int    backup;
extern int    nPattern;
extern char * pattern_baseDir;
extern int    nTargets;
extern int    my_ACK_WAIT_TIMES;

char * my_MCAST_ADDR = MCAST_ADDR;
int  my_FLOW_PORT    = FLOW_PORT;
int  my_PORT         = PORT;
int  my_TTL          = MCAST_TTL;
int  my_LOOP         = MCAST_LOOP;
char * my_IFname     = MCAST_IF;

int           without_monitor = FALSE;
int           monitorID = -1;

int           no_feedback_count;

void usage()
{
  fprintf(stderr, 
	  "multicaster  (to copy files to many multicatchers) version %s)\n"
	  "   Option list:\n"
	  "   [ -v <verbose_level 0-2> ]\n"
	  "   [ -w <ack_wait_times default= %d (secs) ]\n"
          "   -------- essential options ----------------------------------\n"
	  "     -m <machine_list_filePath>\n" 
	  "     -s <data_source_path>\n"
	  "     -f <synclist_filePath>\n" 
	  "   -------- options for backup ---------------------------------\n"
	  "   [ -b flag to turn on backup ]\n"
	  "   [ -r <filepath> for regex patterns for files needing backup ]\n"
	  "   [ -d <basedir> for regex patterns ]\n"
	  "   [ -x flag to turn off feedback monitoring ]\n"
	  "   -------- mcast options --------------------------------------\n"
	  "   [ -A <my_mcast_address default=%s> **same as for multicatcher ]\n"
	  "   [ -P <my_PORT default=%d> **same as for multicatcher ]\n"          
	  "   [ -T <my_TTL default=%d> ]\n"
	  "   [ -L flag turn on mcast_LOOP. default is off ]\n"
	  "   [ -I <my_MCAST_IF default=NULL> ]\n",
	  VERSION, my_ACK_WAIT_TIMES, MCAST_ADDR, PORT, my_TTL);
}

int monitor_cmd(int cmd, int machineID)
{
  int count =0;
  set_delay(0, FAST);
  send_cmd(cmd, machineID); 
  while (1) { /* wait for ack */
    if (read_handle_complaint()==0) {
      ++count;
      send_cmd(cmd, machineID); 
      if (count > 100) return FALSE;  /* wait time ~ 100 * FAST */
    } else 
      return TRUE;
  }
}

int find_max_missing_machine(char *flags)
{
  int i, index = -1;
  int max = -1;
  int threshold;

  for(i=0; i<nTargets; ++i) {
    int missing;
    missing = total_missing_page[i];
    if (missing > max && flags[i] == GOOD_MACHINE) {
      max = missing;
      index = i;
    }
  }

  threshold = (get_nPages() > SWITCH_THRESHOLD*100) ?
    get_nPages() / 100 : SWITCH_THRESHOLD;

  return ((index >= 0) && 
	  (monitorID >= 0) &&
	  (bad_machines[monitorID] == GOOD_MACHINE) &&
	  (max <= (total_missing_page[monitorID] + threshold))) ?
    monitorID : index;
}

void check_change_monitor(int undesired_index)
{
  /* this function changes the value of the global var: monitorID */
  int i, count = 0;
  char * flags;

  if (without_monitor) {
    if (verbose >=1) printf("No monitor is selected. i.e. no congestion control.\n");
    return; 
    /* in this case, no target machine
       is selected as the monitor (isMonitor=false for all).
       So, the feedback mechanism is effectively turned off*/
  }

  flags = malloc(nTargets * sizeof(char));
  for(i=0; i<nTargets; ++i) {
    flags[i] = (i==undesired_index) ? BAD_MACHINE :bad_machines[i]; 
  }

  while (count < nTargets) {
    i = find_max_missing_machine(flags);
    if (i==monitorID) break;
    monitorID = i;

    if (monitorID < 0) break;

    if (monitor_cmd(SELECT_MONITOR_CMD, monitorID)) {
      if (verbose >=1) printf("Monitor = %s\n", id2name(monitorID));
      break;
    } else {
      flags[monitorID] = BAD_MACHINE;
    }
    ++count;
  }

  free(flags);
  if (monitorID < 0) {
    fprintf(stderr, "monitor machine cannot be set up!\n");
    do_cntl_c(0);
    my_exit(BAD_EXIT);
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
       at this point, the readable() returns without getting a reply 
       from monitorID after FACTOR*DT_PERPAGE (or DT_PERPAGE if without_monitor)
    */
    if (without_monitor) return;

    ++no_feedback_count;
    if (verbose>=2) printf("no reply, count = %d\n", no_feedback_count);
    update_rtt_hist(999999);  /* register this as rtt = infinite --- the last element in rtt_hist */
    if (no_feedback_count>NO_FEEDBACK_COUNT_MAX) { /* count the consecutive no_feedback event */
      /* switch to another client */
      if (verbose >=2) 
	fprintf(stderr, "Consecutive non_feedback exceeds limit, Changing monitor machine.\n");
      if (nTargets>1) check_change_monitor(monitorID); /* replace the current monitor */
      no_feedback_count = 0;
      /* for nTargets = 1,
	 there is a danger of hanging in this loop...
	 add more checking later
      */
    }
  } else {
    end_timer();
    update_time_accumulator();
    rtt = get_accumulated_usec();
    /* to do: wait additional time after receiving feedback: usleep( rtt * 0.1 ); */
    /* to do: update histogram */
    if (verbose >=2) printf("rtt(p = %d) = %ld (usec)\n", page, rtt);
    update_rtt_hist(rtt);
    
    no_feedback_count = 0;
  }
}

void send_cmd_and_wait_ack(int cmd_code)
{
  send_cmd(cmd_code, (int) ALL_MACHINES);
  refresh_machine_status();
  /*set_delay(0, FAST);*/
  set_delay(0, DT_PERPAGE* ((without_monitor)? 1 : FACTOR));
  wait_for_ok(cmd_code);
  do_badMachines_exit();
  check_change_monitor(-1);
}



int main(int argc, char *argv[])
{
  char c;
  int cfile, ctotal_pages, cpage;
  int bad_args = FALSE;
  char * source_path       = NULL;
  char * synclist_path     = NULL;
  char * machine_list_file = NULL;
  time_t tloc;
  time_t time0, time1; 

  while ((c = getopt(argc, argv, "v:w:A:P:T:LI:m:s:f:xbr:d:")) !=  EOF) {
    switch (c) {
    case 'v': 
      verbose = atoi(optarg);
      break;
    case 'w':
      my_ACK_WAIT_TIMES = atoi(optarg);
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
    case 'm':
      machine_list_file = optarg;
      break;
    case 's':
      source_path = optarg;      
      break;
    case 'f':
      synclist_path = optarg;    
      break;
    case 'b':
      backup = TRUE; /* if nPattern==0, backup means back up ALL files */
      break;
    case 'r':        /* to selectively back up certain files as defined in the pattern */
      if (!read_backup_pattern(optarg)) {
	fprintf(stderr, "Failed in loading regex patterns in file = %s\n", optarg);
	bad_args = TRUE;
      }
      break;
    case 'd':
      pattern_baseDir = strdup(optarg);
      if (pattern_baseDir[strlen(pattern_baseDir)-1]=='/') 
	pattern_baseDir[strlen(pattern_baseDir)-1] = '\0' ; /* remove last / */
      break;
    case 'x':
      /* flag to indicate if we want to use the congestion control */
      without_monitor = TRUE;
      break;
    case '?':
      usage();
      bad_args = TRUE;
    }
  }

  if (!machine_list_file || !source_path || !synclist_path ) {
    fprintf(stderr, "Essential options (-m -s -f) should be specified. \n");
    usage();
    bad_args = TRUE;
  }

  if (nPattern>0) backup = TRUE;
  if (backup && nPattern>0) {
    if (!pattern_baseDir) pattern_baseDir = strdup(source_path);
    if (strlen(source_path) < strlen(pattern_baseDir) ||
	strncmp(source_path, pattern_baseDir, strlen(pattern_baseDir))!=0) {
      fprintf(stderr, "src_path (%s) should include (and be longer than) pattern_baseDir (%s)",
	      source_path, pattern_baseDir);
      bad_args = TRUE; /* cannot do my_exit() here, since init_send() is not run */
    }
  }

  get_machine_names(machine_list_file);
  if (!init_synclist(synclist_path, source_path)) bad_args = TRUE;

  if (verbose >= 2)
    fprintf(stderr, "Total number of files: %d\n", total_entries());

  /* init the network stuff and some flags */
  init_sends();
  if (bad_args) my_exit(BAD_EXIT);
  init_complaints();
  init_machine_status(nTargets);

  /* set up Cntl_C catcher */
  Signal(SIGINT, do_cntl_c);

  /* ------------------- set up monitor machine for doing feedback for each page sent */
  check_change_monitor(-1);
  
  /*-------------------------------------------------------------------------------*/

  init_rtt_hist();
  time0 = time(&tloc);  /* start time */

  /* -----------------------------Send the file one by one -----------------------------------*/
  for (cfile = 1; cfile <= total_entries(); cfile++) { /* for each file to be synced */
    if (!get_next_entry(cfile)) continue;

    ctotal_pages = pages_for_file();

    /* 
       By the time this file, which was obtained when synclist was
       established some time ago, may no longer exist on the master.
       So, we need to check the existence of this file.
       fexist() also opens the file so that it won't be deleted 
       between here and the send-page-loop.
    */
    if (ctotal_pages > 0  && (!same_stat_for_file() ||
			      !fexist(current_entry()))) { 
      /* go to next file if this file has changed or does not exist */
      fprintf(stderr, "%s (%d out of %d; Extinct file)\n", 
	      getFilename(), current_entry(), total_entries());
      adjust_totals();
      continue;
    }
 
    if (ctotal_pages < 0) {
      fprintf(stderr, "%s (%d out of %d; to delete)\n", 
	      getFilename(), current_entry(), total_entries());
    } else {
      fprintf(stderr, "%s (%d out of %d; %d pages)\n", 
	      getFilename(), current_entry(), total_entries(), ctotal_pages);
    }

    /* send_open_cmd */
    pack_open_file_info();
    send_cmd_and_wait_ack(OPEN_FILE_CMD);

    /* 
       ctotal_pages < 0, for deletion 
       ctotal_pages = 0, regular file with no content.
                         or directory, softlink, hardlink
       both should have been finished with OPEN_FILE_CMD 
    */  
    if (ctotal_pages <= 0) continue;  

    /* for other regular files */    
    set_delay(0, DT_PERPAGE* ((without_monitor)? 1 : FACTOR));
    init_missing_page_flag(ctotal_pages);
        
    /* ----- sending file data ----- first round */
    no_feedback_count = 0;
    for (cpage = 1; cpage <= ctotal_pages; cpage++) {
      /* 
	 the mode field may be changed by change_monitor() 
	 so, we have to set the mode field for every page
      */
      set_mode(SENDING_DATA);  
      do_one_page(cpage);
    }

    /* EOF */
    reset_has_missing();
    send_cmd_and_wait_ack(EOF_CMD);

    /* ----- sending file data again, 2nd and 3rd and ...n-th round */
    while (has_missing_pages()) {
      set_delay(0, DT_PERPAGE*((without_monitor)? 1 : FACTOR)); 
      no_feedback_count = 0;

      for (cpage = 1; cpage <= ctotal_pages; cpage++){
	if (is_it_missing(cpage-1)) {
	  set_mode(RESENDING_DATA);  
	  do_one_page(cpage);
	  page_sent(cpage-1);
	}
      }

      /* eof */
      reset_has_missing();
      send_cmd_and_wait_ack(EOF_CMD);
    };

    /* close file */
    send_cmd_and_wait_ack(CLOSE_FILE_CMD);
    free_missing_page_flag();
  } /* end of the for each_file loop */

  time1= time(&tloc);
  return (send_done_and_pr_msgs( ((double)(time1 - time0))/ 60.0 ));
}

