/*
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>  
   Copyright (C)  2005 Renaissance Technologies Corp.
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
                  Codes in this file are extracted and modified from multicatcher.c.

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

char * my_MCAST_ADDR = MCAST_ADDR;
int  my_FLOW_PORT    = FLOW_PORT;
int  my_PORT         = PORT;
char * my_IFname     = MCAST_IF;

void usage()
{
  fprintf(stderr, 
	  "rttcatcher (to receive pages on the target. version %s\n"
	  "   Option list:\n"
	  "   [ -v flag to turn on verbose]\n"
	  "   -------- mcast options --------------------------------------\n"
	  "   [ -A <my_mcast_address default=%s)> ]\n"
	  "   [ -P <my_PORT default=%d> ]\n"
	  "   [ -I <my_MCAST_IF default=NULL> ]\n",
	  VERSION, MCAST_ADDR, PORT);
}

int main(int argc, char *argv[])
{
  int old_mode;   /* hp: from char to int for mode */
  int mode;
  int c;

  verbose     = 0;
  while ((c = getopt(argc, argv, "vA:P:I:")) !=  EOF) {
    switch (c) {
    case 'v': 
      verbose = 1;
      break; 
    case 'A':
      my_MCAST_ADDR = optarg;
      break;
    case 'P':
      my_PORT  = atoi(optarg);
      my_FLOW_PORT = my_PORT -1;
      break; 
    case 'I':
      my_IFname = optarg;
      break;
    case '?':
      usage();
      exit(-1);
    }
  }

  init_page_reader();
  init_complaint_sender();

  /* initialize random numbers */
  srand(time(NULL) + getpid());

  /* Wait forever if necessary for first packet */
  set_delay(0, -1);
  mode = old_mode = TEST;   /* hp: add mode */

  while(1) {  /* loop for all incoming pages  */
    if (verbose)
      fprintf(stderr, "Starting listen loop with mode %d\n", mode);

    mode = read_handle_page();
    if (verbose) fprintf(stderr, "in mode %d\n", mode);

    if (mode == ALL_DONE_CMD) break;

    /* got no data? */
    if (mode == TIMED_OUT) {
      if (verbose) fprintf(stderr, "*");
      continue;
    } /* end if TIMED_OUT */

    /* changing modes? */
    if ((old_mode != SENDING_DATA) && (mode == SENDING_DATA)){
      /* Taking data,  wait at least 3 to 8 seconds */
      set_delay( 3 + rand() % 8, 200);
      if (verbose) fprintf(stderr, "Receiving data\n");
      old_mode = mode;
      continue;
    }
  
    /* all other modes */
    old_mode = mode;

  } /* end of incoming page loop */

  if (verbose) fprintf(stderr, "Done!\n");
  return 0;
}


