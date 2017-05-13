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

#ifndef __main_proto_h
#define __main_proto_h

/* parse_synclist.c */
unsigned int total_entries();
unsigned int fill_in_stat(char *buf);
unsigned int fill_in_linktar(char *buf);
unsigned int fill_in_filename(char * buf);
unsigned int get_nPages();
int    pages_for_file();
char * getFilename();
char * getFullname();
int    same_stat_for_file();
void   strip(char * str);
int    current_entry();
int    get_next_entry(int current_file_id);
int    is_softlink();
int    is_directory();
int    is_hardlink();
int    init_synclist(char * synclist_path, char *bdir);
void   adjust_totals();

/* backup.c */
int    read_backup_pattern(char * fpat_file);
int    needBackup(char * filename);

/* sends.c */
void   init_sends();
void   set_mode(int new_mode);   
int    send_page(int page);
void   send_test();
void   send_cmd(int code, int machine_id);
void   send_all_done_cmd();
int    fexist(int entry) ;
void   pack_open_file_info();
void   my_exit(int);

/* complaints.c */
void   init_complaints();
int    read_handle_complaint(int cmd);
void   wait_for_ok(int code);
void   refresh_machine_status();
void   refresh_missing_pages();
void   mod_machine_status();
void   refresh_file_received();
int    nNotRecv();
int    iNotRecv();
int    is_it_missing(int page);
int    has_missing_pages();
int    has_sick_machines();
void   init_missing_page_flag(int n);
void   free_missing_page_flag();
void   refresh_machine_status();
void   init_machine_status(int n);
void   page_sent(int page);
int    nBadMachines();
void   do_badMachines_exit();
int    pr_missing_pages();
int    send_done_and_pr_msgs(double);
void   do_cntl_c(int signo);
void   set_has_missing();
void   reset_has_missing();
void   set_has_sick();
void   reset_has_sick();


/* setup_socket.c */
void   set_delay(int secs, int usecs);
void   get_delay(int * secs, int * usecs);
int    readable(int fd);
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

/* complaint_sender.c */
void   send_complaint(int complaint, int mid, int page, int file); 
void   init_complaint_sender();
#ifndef IPV6
void update_complaint_address(struct sockaddr_in *sa);
#else
void update_complaint_address(struct sockaddr_in6 *sa);
#endif

/* page_reader.c */
void   init_page_reader();
int    check_queue();
int    read_handle_page();

/* file_operations.c */
void   get_tmp_suffix();
int    extract_file_info(char * buf, int n_file, unsigned int n_pages);
int    open_file();
int    close_file();
int    rm_tmp_file();
int    delete_file();
int    touch_file();
int    nPages_for_file();
int    has_all_pages();
int    ask_for_missing_page();
void   write_page(int page, char* data_ptr, int bytes);
int    is_missing(int page);
void   page_received(int page);
int    set_owner_perm_times();
void   close_last_file();   
int    check_zero_page_entry();
void   default_suffix();

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
unsigned int pages_wo_ack();

/* signal.c */
typedef	void	Sigfunc(int);	/* for signal handlers */
Sigfunc * Signal(int signo, Sigfunc *func);
int    Fcntl(int fd, int cmd, int arg);
int    Ioctl(int fd, int request, void *arg);
void   Sigemptyset(sigset_t *set);
void   Sigaddset(sigset_t *set, int signo);
void   Sigprocmask(int how, const sigset_t *set, sigset_t *oset);

/* id_map.c */
void   get_machine_names(char * filename);
char * id2name(int id);

#endif
