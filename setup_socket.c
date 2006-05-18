/*  
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>
		  make it IPv6-ready
   Copyright (C)  2005 Renaissance Technologies Corp.
                  file name is changed from main.c to setup_socket.c
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
/*  
   This part was based on 
   (1) codes by Aaron Hillegass <aaron@classmax.com>
   (2) codes in Steven's book 'network programming'

   200605 change it to make it IPv6 ready

*/

#include        <sys/socket.h>
#include        <sys/types.h>
#include        <sys/un.h>
#include        <stdio.h>
#include        <stdlib.h>
#include	<netinet/in.h>	/* sockaddr_in{} and other Internet defns */
#include	<arpa/inet.h>	/* inet(3) functions */
#include	<errno.h>

extern int verbose;
int delay_sec;
int delay_usec;


/*  Set up for catcher's complaint socket */
#ifndef IPV6
int complaint_socket(struct sockaddr_in *addr, int port)
#else
int complaint_socket(struct sockaddr_in6 *addr, int port)
#endif
{  
  int fd, sockaddr_len;
  sa_family_t family;
  if (verbose>=2) fprintf(stderr, "in send_socket_ip\n");

  #ifndef IPV6 
  /* IPv4 */
  sockaddr_len = sizeof(struct sockaddr_in);
  memset(addr, 0, sockaddr_len);
  addr->sin_family = AF_INET;
  family = AF_INET;
  addr->sin_port = htons(port);
  /*addr->sin_addr.s_addr = ip;      this fx in for init process only 
                                     This ip will be overwritten later after
				     1st packet is received. */
  #else
  /* IPv6 */
  sockaddr_len = sizeof(struct sockaddr_in6);
  memset(addr, 0, sockaddr_len);
  addr->sin6_family = AF_INET6;
  family = AF_INET6;
  addr->sin6_port = htons(port);
  /* addr->sin_addr.s_addr = ip;     see comments above */
  #endif

  if ((fd = socket(family, SOCK_DGRAM, 0)) < 0){
    perror("Send socket");
    exit(1);
  }
  return fd;
}

/*  Set up mcast send socket for multicaster based on (char*)cp */
#ifndef IPV6 
int send_socket(struct sockaddr_in *addr, char * cp, int port)
#else
int send_socket(struct sockaddr_in6 *addr, char * cp, int port)
#endif
{
  int fd, sockaddr_len;
  sa_family_t family;
  char buf[50];
  if (verbose>=2) fprintf(stderr, "in send_socket\n");

  #ifndef IPV6 
  /* IPv4 */
  sockaddr_len = sizeof(struct sockaddr_in);
  memset(addr, 0, sockaddr_len);
  addr->sin_family = AF_INET;
  family = AF_INET;
  addr->sin_port = htons(port);
  /*addr->sin_addr.s_addr = inet_addr(cp);*/
  inet_pton(AF_INET, cp, &addr->sin_addr);
  /* Print out IP address and port */
  inet_ntop(AF_INET, &addr->sin_addr, buf, 50);
  #else
  sockaddr_len = sizeof(struct sockaddr_in6);
  memset(addr, 0, sockaddr_len);
  addr->sin6_family = AF_INET6;
  family = AF_INET6;
  addr->sin6_port = htons(port);
  /*addr->sin_addr.s_addr = inet_addr(cp);*/
  inet_pton(AF_INET6, cp, &addr->sin6_addr);
  /* Print out IP address and port */
  inet_ntop(AF_INET6, &addr->sin6_addr, buf, 50);
  #endif
   
  if (verbose>=2)
    fprintf(stderr, "Creating a send socket to %s:%d\n", buf, port);

  if ((fd = socket(family, SOCK_DGRAM, 0)) < 0){
    perror("Send socket");
    exit(1);
  }

  if ((bind(fd, (const struct sockaddr *)addr, sockaddr_len)) < 0){
       perror("in send_socket(): bind error (need to change MCAST_ADDR)");
       exit(1);
     }
  return fd; /*send_socket_ip(addr, address, port);*/
}

/* set up socket on the receiving end */
#ifndef IPV6 
int rec_socket(struct sockaddr_in *addr, int port)
#else
int rec_socket(struct sockaddr_in6 *addr, int port)
#endif
{
  int fd, sockaddr_len;
  sa_family_t family;

  #ifndef IPV6 
  /* IPv4 */
  sockaddr_len = sizeof(struct sockaddr_in);
  memset(addr, 0, sockaddr_len);
  addr->sin_family = AF_INET;
  family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);
  #else
  sockaddr_len = sizeof(struct sockaddr_in6);
  memset(addr, 0, sockaddr_len);
  addr->sin6_family = AF_INET6;
  family = AF_INET6;
  addr->sin6_port = htons(port);
  addr->sin6_addr = in6addr_any;  /* RS book: page 92 */
  #endif
     
  if((fd = socket(family, SOCK_DGRAM, 0)) < 0){
    perror("Socket create");
    exit(1);
  }
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

  if (verbose>=2)
    fprintf(stderr, "Creating receive socket on port %d\n", port);
  
  /*if ((bind(fd, addr, sizeof(*addr))) < 0){  MOD by RWM: replaced by next line */
  if ((bind(fd, (const struct sockaddr *)addr, sockaddr_len)) < 0){
    perror("in rec_socket(): bind error (need to change PORT)");
    exit(1);
  }
  return fd;
}

/* 
   set the values of two variables to be used by select()
   in readable().
*/
void set_delay(int secs, int usecs)
{
  if (verbose>=2) {
    if (usecs == -1)
      fprintf(stderr, "Timeout: set to infinite\n");
    else
      fprintf(stderr, "Timeout: set to %d sec + %d usec\n", secs, usecs);
  }
  delay_sec = secs;
  delay_usec = usecs;
}

/* 
   Check if there is an incoming UDP for a certain amount
   of time period specified in delay_tv..
  
   Return 'true' if there is.
   Return 'false' if no valid UDP has arrived within the time period.

   In multicaster, this is used in read_handle_page() right after
   send_page() is carried out. As a result, read_handle_complaint() waits
   for any incoming complaints from any target machines within
   the specified time period. As it is now, no target machine
   will send back complaints during the transmission of all the
   pages for a file. Therefore, read_handle_complaint() serves effectively
   as a time delay between sending of each page.
*/
int readable(int fd)
{
  struct timeval delay_tv;
  fd_set   rset;
  FD_ZERO(&rset);
  FD_SET(fd, &rset);

  /* 
     if microsec == -1 wait forever for a packet.
     This is used in the beginning when multicatcher is just
     invoked.
  */
  if (delay_usec == -1){
    return(select(fd + 1, &rset, NULL, NULL, NULL));
  } else {
    delay_tv.tv_sec = delay_sec;
    delay_tv.tv_usec = delay_usec;
    return (select(fd + 1, &rset, NULL, NULL, &delay_tv));
  }
}

