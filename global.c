/*  
   Copyright (C)  2006 Renaissance Technologies Corp.
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

char *cmd_name[] = { "TIME-OUT",            
		     "TEST",  
		     "SEND_DATA",
		     "RESEND_DATA",
		     "OPEN_FILE", 
		     "EOF",
		     "CLOSE_FILE",
		     "CLOSE_ABORT",
		     "ALL_DONE",
		     "SEL_MONITOR",
		     "NULL"};

int    verbose = 0;    /* = 0 little detailed output, = 1,2 ... = n a lot more details */
int    machineID = -1; /* used for multicatcher */

