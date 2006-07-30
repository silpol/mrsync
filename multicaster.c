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
                version 3.2.0
                  -- monitor change improvement
                  -- handshake improvement (e.g. seq #)
		  -- if one machine skips a file, all will NOT close()

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
extern char * file_received;
extern int  * missing_pages; 
extern int    backup;
extern int    nPattern;
extern char * pattern_baseDir;
extern int    nTargets;
extern int    my_ACK_WAIT_TIMES;
extern int    toRmFirst;
extern int    quitWithOneBad;
extern int    skip_count;
extern int    file_changed;

char * my_MCAST_ADDR = MCAST_ADDR;
int  my_FLOW_PORT    = FLOW_PORT;
int  my_PORT         = PORT;
int  my_TTL          = MCAST_TTL;
int  my_LOOP         = MCAST_LOOP;
char * my_IFname     = MCAST_IF;

int           monitorID = -1;

int           no_feedback_count;

void usage()
{
  fprintf(stderr, 
	  "multicaster  (to copy files to many multicatchers) version %s)\n"
	  "   Option list:\n"
	  "   [ -v <verbose_level 0-2> ]\n"
	  "   [ -w <ack_wait_times default= %d (secs) ]\n"
	  "   [ -X toRmFirst_flag default= cp2tmp_and_mv ]\n"
	  "   [ -q quit_with_1_bad defalut= quit_with_all_bad ]\n"
          "   -------- essential options ----------------------------------\n"
	  "     -m <machine_list_filePath>\n" 
	  "     -s <data_source_path>\n"
	  "     -f <synclist_filePath>\n" 
	  "   -------- options for backup ---------------------------------\n"
	  "   [ -b flag to turn on backup ]\n"
	  "   [ -r <filepath> for regex patterns for files needing backup ]\n"
	  "   [ -d <basedir> for regex patterns ]\n"
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
  /* 
     Once this fx is called the old monitor will be 
     turned off. So, we need to make sure this fx
     returns TRUE for a machine. Or the monitor_set_up
     should fail. 
  */
  int count =0;
  set_delay(0, FAST);
  send_cmd(cmd, machineID); 
  while (1) { /* wait for ack */
    if (read_handle_complaint(cmd)==0) {
      ++count;
      send_cmd(cmd, machineID); 
      if (count > SET_MON_WAIT_TIMES) {
	return FALSE;
      }
    } else {
      return TRUE;
    }
  }
}

int find_max_missing_machine(char *flags)
{
  /* flags[] -> which machines are not considered */
  int i, index = -1;
  int max = -1;
  int threshold;

  for(i=0; i<nTargets; ++i) {
    int missing;
    /** missing = total_missing_page[i];    **/
    missing = missing_pages[i];  /* for last file (OPEN) or this file (other cmds)*/
    if (missing > max && flags[i] == GOOD_MACHINE) {
      max = missing;
      index = i;
    }
  }

  threshold = (get_nPages() > SWITCH_THRESHOLD*100) ?
    get_nPages() / 100 : SWITCH_THRESHOLD;
  if (index>=0) {
    if (monitorID>=0) {
      if (flags[monitorID]==BAD_MACHINE) return index;
      /** if (max <= (total_missing_page[monitorID] + threshold)) **/
      if (max <= (missing_pages[monitorID] + threshold))
	return monitorID;
    }
    return index;
  } else { /* could come here if all are busy*/
    return -1;
  }

  /**
  return ((index >= 0) && 
	  (monitorID >= 0) &&
	  (bad_machines[monitorID] == GOOD_MACHINE) &&
	  (max <= (total_missing_page[monitorID] + threshold))) ?
    monitorID : index;
  **/
}

void set_monitor(int mid)
{
  /* one machine is to be set. So need to succeed. */
  if (monitor_cmd(SELECT_MONITOR_CMD, mid)) {
    if (verbose >=1) fprintf(stderr, "Monitor - %s\n", id2name(mid));
    return;
  } else {
    fprintf(stderr, "Fatal: monitor %s cannot be set up!\n", id2name(mid));
    send_done_and_pr_msgs(-1.0);  
    exit(BAD_EXIT);
  }
}

void check_change_monitor(int undesired_index)
{
  /* this function changes the value of the global var: monitorID */
  int i, count;
  char * flags;
 
  /* if all targets received the file, no need to go on */
  if ((count=nNotRecv())==0) return;

  if (count==1) {
    i = iNotRecv();        
    if (bad_machines[i] == GOOD_MACHINE) {
      monitorID = i;
      /* 
	 'i' could be the current monitor.
	 We'd like to set it because there might
	 be something wrong with it if we come to 
	 to this point.
      */
      set_monitor(monitorID);
    }    
    return;
  }

  /* more than two machines do not receive the file yet */
  /* flags mark those machines as BAD which we don't want to consider */
  flags = malloc(nTargets * sizeof(char));
  for(i=0; i<nTargets; ++i) {
    flags[i] = (i == undesired_index || file_received[i] == FILE_RECV) ? 
      BAD_MACHINE : bad_machines[i]; 
  }

  /* check if all machines are not to be considered */
  count = 0;
  for(i=0; i<nTargets; ++i) {
    if (flags[i]==BAD_MACHINE) ++count;
  }
  if (count==nTargets) {
    /* Two ways to get here are
       (1) during do_one_page:
           prev_monitor = (not_recv) and other not_recv's are bad_machines.           
       (2) during after-ack:
           prev_monitor = (recv) and other not_recv's are bad_machines.
    */
    free(flags);
    set_monitor(monitorID);
    return; 
  }

  /* at least one not_recv (and good) machine is to be considered */
  count = 0;
  while (count < nTargets) {
    i = find_max_missing_machine(flags);
    /* if (i==monitorID) break;*/
    monitorID = i;

    if (monitorID < 0) break;

    if (monitor_cmd(SELECT_MONITOR_CMD, monitorID)) {
      if (verbose >=1) fprintf(stderr, "Monitor = %s\n", id2name(monitorID));
      break;
    } else {
      flags[monitorID] = BAD_MACHINE;
      /* Then, we attemp to set up other machine */
    }
    ++count;
  }

  free(flags);
  if (monitorID < 0) {
    fprintf(stderr, "Fatal: monitor machine cannot be set up!\n");
    send_done_and_pr_msgs(-1.0);  
    exit(BAD_EXIT);
  }
}

void do_one_page(int page)
{
  unsigned long rtt;
  refresh_timer();
  start_timer();
  if (!send_page(page)) return;

  /* read_handle_complaint() waits n*interpage_interval at most */
  if (read_handle_complaint(SENDING_DATA)==0) {  
    /* delay_sec for readable() is set by set_delay() */
    /* 
       at this point, the readable() returns without getting a reply 
       from monitorID after FACTOR*DT_PERPAGE (or DT_PERPAGE if without_monitor)
    */

    ++no_feedback_count;
    if (verbose>=2) printf("no reply, count = %d\n", no_feedback_count);
    update_rtt_hist(999999);  
    /* register this page as rtt = infinite --- the last element in rtt_hist */

    if (no_feedback_count > NO_FEEDBACK_COUNT_MAX) { 
      /* switch to another client */
      if (verbose >=2)
	fprintf(stderr, 
		"Consecutive non_feedback exceeds limit, Changing monitor machine.\n");
      /* if (nTargets>1 && (nTargets - nBadMachines()) > 1 && nNotRecv() > 1) */
      check_change_monitor(monitorID); /* replace the current monitor */
      no_feedback_count = 0;
    }
    return;
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
  set_delay(0, DT_PERPAGE*FACTOR);
  if (cmd_code==EOF_CMD) mod_machine_status(); 
  wait_for_ok(cmd_code);
  do_badMachines_exit();
  /* check_change_monitor(-1); */
}

int do_file_changed_skip()
{
  /* if file is changed during syncing, then we should skip this file */
  if (file_changed || !same_stat_for_file()) {
    fprintf(stderr, "WARNING: file is changed during sycing -- skipping\n");
    send_cmd_and_wait_ack(CLOSE_ABORT_CMD);
    free_missing_page_flag();
    ++skip_count;
    return TRUE;
  }
  return FALSE;
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

  while ((c = getopt(argc, argv, "v:w:A:P:T:LI:m:s:f:br:d:Xq")) !=  EOF) {
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
    case 'X':
      toRmFirst = TRUE;
      break;
    case 'q':
      quitWithOneBad = TRUE;
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
      fprintf(stderr, 
	      "src_path (%s) should include (and be longer than) pattern_baseDir (%s)",
	      source_path, pattern_baseDir);
      bad_args = TRUE; /* cannot do my_exit() here, since init_send() has not run */
    }
  }

  if (backup && toRmFirst) {
    fprintf(stderr, "-B and -X cannot co-exist\n");
    bad_args = TRUE;
  }

  if (machine_list_file) { 
    get_machine_names(machine_list_file);
    if (!init_synclist(synclist_path, source_path)) bad_args = TRUE;
  }

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
    init_missing_page_flag(ctotal_pages);
    refresh_missing_pages(); /* total missing pages for this file for each tar */
        
    /* ----- sending file data ----- first round */
    no_feedback_count = 0;    
    for (cpage = 1; cpage <= ctotal_pages; cpage++) {
      /* 
	 the mode field and delay may be changed by change_monitor
      */
      set_delay(0, DT_PERPAGE*FACTOR);
      set_mode(SENDING_DATA);  
      do_one_page(cpage);
    }

    if (do_file_changed_skip()) continue;

    /* send "I am done with the first round" */
    reset_has_missing();
    refresh_file_received(); /* to record machines that have received this file */
    send_cmd_and_wait_ack(EOF_CMD);
    
    /* after the first run, before we go to 2nd and 3rd run, */
    if (has_missing_pages()) check_change_monitor(-1);  

    /* ----- sending file data again, 2nd and 3rd and ...n-th round */
    reset_has_sick();
    while (has_missing_pages()) {
      int c; /****************/      
      no_feedback_count = 0;

      c = 0;
      for (cpage = 1; cpage <= ctotal_pages; cpage++){
	if (is_it_missing(cpage-1)) {
	  set_delay(0, DT_PERPAGE*FACTOR); 
	  set_mode(RESENDING_DATA);  
	  do_one_page(cpage);
	  page_sent(cpage-1);
	  ++c; /*************/
	}
      }
      fprintf(stderr, "re-sent N_pages = %d\n", c); /*************/

      /* eof */
      reset_has_missing();
      send_cmd_and_wait_ack(EOF_CMD);
      if (has_sick_machines()) {
	break;
	/* one machine can reach sick_state while some others are still
	   in missing_page state.
	   This break here is ok in terms of skipping this file.*/
      } else {
	check_change_monitor(-1);
      }
    };

    if (do_file_changed_skip()) continue;

    /* close file */
    send_cmd_and_wait_ack((has_sick_machines()) ? CLOSE_ABORT_CMD : CLOSE_FILE_CMD);
    if (has_sick_machines()) {
      fprintf(stderr, "Skip_syncing %s\n", getFilename());
    }
    free_missing_page_flag();
  } /* end of the for each_file loop */

  time1= time(&tloc);
  return (send_done_and_pr_msgs( ((double)(time1 - time0))/ 60.0 ));
}

