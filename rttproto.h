/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
   Copyright (C)  2005 Renaissance Technologies Corp. 
   Copyright (C)  2001 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
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

#ifndef __rttproto_h
#define __rttproto_h

/* rttsends */
void init_sends(int n);
void set_mode(int new_mode);   
int  send_page(int page);
void send_cmd(int code, int machine_id);
void send_all_done_cmd();

/* rttcomplaints */
void init_complaints();
int  read_handle_complaint();
void wait_for_ok(int code);
void refresh_machine_status();
int  is_it_missing(int page);
int  has_missing_pages();
void init_missing_page_flag(int n);
void free_missing_page_flag();
void refresh_machine_status();
int  get_total_missing_pages();
void page_sent(int page);
void pr_missing_pages();
void do_cntl_c(int signo);
void send_done_and_pr_msgs();
void do_badMachines_exit(char * machine, int pid);

/* setup_socket.c */
void set_delay(int secs, int usecs);
int  readable(int fd);
#ifndef IPV6
int complaint_socket(struct sockaddr_in *addr, int port);
int send_socket(struct sockaddr_in *addr, char * cp, int port);
int rec_socket(struct sockaddr_in *addr, int port);
#else
int rec_socket(struct sockaddr_in6 *addr, int port);
int send_socket(struct sockaddr_in6 *addr, char * cp, int port);
int complaint_socket(struct sockaddr_in6 *addr, int port);
#endif

/* set_mcast.c */
int mcast_set_if(int sockfd, const char *ifname, u_int ifindex);
int mcast_set_loop(int sockfd, int onoff);
int mcast_set_ttl(int sockfd, int val);

/* set_catcher_mcast.c */
int Mcast_join(int sockfd, const char *mcast_addr,
	       const char *ifname, u_int ifindex);
void sock_set_addr(struct sockaddr *sa, socklen_t salen, const void *addr);

/* rttcomplaint_sender */
void send_complaint(int complaint, int page);  
void init_complaint_sender();
#ifndef IPV6
void update_complaint_address(struct sockaddr_in *sa);
#else
void update_complaint_address(struct sockaddr_in6 *sa);
#endif

/* rttpage_reader */
void init_page_reader();
int  read_handle_page();

/* rttmissings */
int  init_missingPages(int n);
int  missing_pages();
int  is_missing(int page);
void page_received(int page);
int  ask_for_missing_page();
int get_total_pages();

/* timing */
void   refresh_timer();
double get_accumulated_time();
void   start_timer();
void   end_timer();
void   update_time_accumulator();
double get_accumulated_usec();
void   update_rtt_hist(unsigned int rtt);
void   pr_rtt_hist();
void   init_rtt_hist();

/* signal.c */
typedef	void	Sigfunc(int);	/* for signal handlers */
Sigfunc * Signal(int signo, Sigfunc *func);
int    Fcntl(int fd, int cmd, int arg);
int    Ioctl(int fd, int request, void *arg);
void   Sigemptyset(sigset_t *set);
void   Sigaddset(sigset_t *set, int signo);
void   Sigprocmask(int how, const sigset_t *set, sigset_t *oset);

#endif
