/*  
   Copyright (C)  2006 Renaissance Technologies Corp. 
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp.
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

#include <sys/time.h>
#include <stdio.h>

#define N 501       /* effectively set the maximum time in rtt_hist to be 500 msec */

extern int verbose;

/* for timing */
struct timeval tv0, tv1;
unsigned long  usec_acc, sec_acc;   /* accumulator of timing */
unsigned int   rtt_hist[N];         /* rtt_hist[i] = count of rtt within (i, i+1) */

void refresh_timer() 
{
  usec_acc = 0;
  sec_acc  = 0;
}

void start_timer()
{
  struct timezone tz;
  gettimeofday(&tv0, &tz);
}

void end_timer()
{
  struct timezone tz;
  gettimeofday(&tv1, &tz); /* end timer -------- */
}

void update_time_accumulator()
{
  if (tv1.tv_usec<tv0.tv_usec) {
    sec_acc  += (tv1.tv_sec - tv0.tv_sec - 1);
    usec_acc += (1000000 + tv1.tv_usec - tv0.tv_usec);
  } else {
    sec_acc  += (tv1.tv_sec - tv0.tv_sec);
    usec_acc += (tv1.tv_usec - tv0.tv_usec);
  }    
}

double get_accumulated_time()
{
  double sec = sec_acc;
  sec += (usec_acc / 1e6);
  return sec;
}

double get_accumulated_usec()
{
  double usec = usec_acc;
  usec += (sec_acc*1e6);
  return usec;
}

void init_rtt_hist()
{
  int i;
  for(i=0; i<N; ++i) rtt_hist[i] = 0;
}

void update_rtt_hist(unsigned int rtt)
{
  unsigned int index;
  index = rtt / 1000;
  if (index>(N-2)) index = N-1;
  rtt_hist[index]++;
}

void pr_rtt_hist()
{
  int i;
  fprintf(stderr, "rtt histogram\n");
  fprintf(stderr, "msec counts\n");
  fprintf(stderr, "---- --------\n");
  for(i=0; i<N; ++i) {
    if (verbose<=1 && i>10) continue;
    if (rtt_hist[i] != 0) {
      fprintf(stderr, "%4d %u\n", i, rtt_hist[i]);
    }
  }
}

unsigned int pages_wo_ack()
{
  return rtt_hist[N-1];
}
