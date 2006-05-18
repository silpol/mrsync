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

extern int machineID;
extern int verbose;
extern int isMonitor;
extern char * baseDir; 
extern char * cmd_name[];
extern char * backup_suffix;

char * my_MCAST_ADDR = MCAST_ADDR;
char * my_IFname     = MCAST_IF;
int  my_FLOW_PORT    = FLOW_PORT;
int  my_PORT         = PORT;

void usage()
{
  fprintf(stderr, 
	  "multicatcher (to receive files from multicaster) version %s\n"
	  "   Option list:\n"
	  "   [ -v <verbose_level 0-2> ]\n"
	  "   -------- essential options ----------------------------------\n"
	  "     -t <destination Dirpath>\n"
	  "     -i <machineID 0 originated id numbers>\n"
	  "   -------- options for backup ---------------------------------\n"
	  "   [ -u <suffix> for backup files if -b is on in multicaster ]\n"
	  "   -------- mcast options --------------------------------------\n"
	  "   [ -A <my_mcast_address default=%s)> **same as for multicaster ]\n"
	  "   [ -P <my_PORT default=%d> **same as for multicaster ]\n"
	  "   [ -I <my_MCAST_IF default=NULL> ]\n",	  
	  VERSION, MCAST_ADDR, PORT);
}

int main(int argc, char *argv[])
{
  int old_mode;   /* hp: from char to int for mode */
  int mode;
  char c;

  while ((c = getopt(argc, argv, "v:A:P:t:i:u:I:")) !=  EOF) {
    switch (c) {
    case 'v': 
      verbose = atoi(optarg);
      break;
    case 'A':
      my_MCAST_ADDR = optarg;
      break;
    case 'P':
      my_PORT  = atoi(optarg);
      my_FLOW_PORT       = my_PORT -1;
      break; 
    case 'I':
      my_IFname = optarg;
      break;
    case 't':
      baseDir = optarg;
      break;
    case 'i':
      machineID = atoi(optarg);
      break;    
    case 'u':
      backup_suffix = strdup(optarg);
      break;
    case '?':
      usage();
      exit(BAD_EXIT);
    }
  }

  if (machineID < 0 || !baseDir) {
    fprintf(stderr, "Essential options (-t -i) should be specified. \n");
    usage();
    exit(BAD_EXIT);
  }
  if (!backup_suffix) default_suffix();

  init_page_reader();
  init_complaint_sender();

  /* initialize random numbers */
  srand(time(NULL) + getpid());

  /* set the timeout for readable() to be about 3 to 6 seconds 
     Actually, this setting is arbitrary.
     The timeout of readable() does not play a role in
     the logic flow.
   */
  set_delay( 3 + rand() % 6, 0);
  mode = old_mode = TEST;  

  /* -----------------------The main loop---------------------------
     Multicatcher simply waits for any incoming UDP,
     reads and handles it.
     If the UDP contains file content, it is placed in the right place.
     If the UDP contains an instruction, it is carried out.

     Multicatcher never complains unless being told so.
     For example, as it is now, multicatcher does not complain
     about the rate of incoming UDP being too fast to handle.
     If multicatcher cannot keep up with the speed, it just
     loses certain pages in a file which will be reported
     later when multicaster requests acknowledgement.
    ---------------------------------------------------------------- */     
  while(1) {  /* loop for all incoming pages  */
    if (verbose>=2)
      fprintf(stderr, "Starting listen loop with mode %d, old_mode = %d\n", mode, old_mode);

    /* the major task here */
    mode = read_handle_page();
    if (verbose>=2) fprintf(stderr, "new page in mode %d\n", mode);

    if (mode == ALL_DONE_CMD) break;

    /* for debugging purpose */
    if ((old_mode != mode)) {
      if (verbose>=2 && mode <= 5) fprintf(stderr, "%s\n", cmd_name[mode]);
    }
    
    /* got no data? */
    if (mode == TIMED_OUT) {
      if (verbose>=2) fprintf(stderr, "*");      
    } 
   
    old_mode = mode;
  } /* end of incoming page loop */

  if (verbose>=1) fprintf(stderr, "Done!\n");
  return 0;
}
