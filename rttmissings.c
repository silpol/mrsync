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

#include "rttmain.h"

char * missingPages = NULL;            /* array of flags */
int    nPages;

int init_missingPages(int n)
{
  int i;  

  nPages = n;

  /* init missingPages flags */
  if (missingPages != NULL) free(missingPages); /* for second round */
  missingPages =  malloc(sizeof(char) * nPages);  
  for(i=0; i < nPages; ++i)
    missingPages[i] = MISSING;

  return 0;
}

int get_total_pages()
{
  return nPages;
}

int missing_pages()
{
  int result; 
  int i;
 
  result = 0;

  for(i=0; i < nPages; ++i) 
    if ((missingPages[i]) == MISSING) ++result;
  return result;
}

int is_missing(int page)
{
  return (missingPages[page] == MISSING) ?  1 : 0;
}

void page_received(int page)
{
  missingPages[page] = RECEIVED;
}

int ask_for_missing_page()
{
  int i; 
  int n, count;
 
  n = missing_pages();
  if (n == 0) {
    /* send_complaint(EOF_OK, machineID, 0); */
    return 0; /* nothing is missing */
  }
 
  count = 0;
  for(i=0; i < nPages; ++i) {
    if ( missingPages[i] == MISSING ) {
      ++count;
      send_complaint((count==n) ? LAST_MISSING : MISSING_PAGE, i+1);
      usleep(DT_PERPAGE);   
    }
  }
    
  return 1;  /* there is something missing */
}

