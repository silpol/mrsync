/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>

		  This file collects functions related to setting multicast
		  for multicatcher. They are IPv4 and IPv6 ready.
		  By default, we use IPv4.
		  To use IPv6, we need to specify -DIPv6 in Makefile.
		  The functions in this file are collected from
		  Richard Stevens' Networking bible: Unix Network programming

		  I added Mcast_join().

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

#include	"main.h"
#include	<net/if.h>
#ifdef _SUN
#include <sys/sockio.h>  /* define SIOCGIFADDR */
#else
#include <linux/sockios.h>
#endif

#define SA      struct sockaddr

int mcast_join(int sockfd, const SA *sa, socklen_t salen,
	       const char *ifname, u_int ifindex)
{
	switch (sa->sa_family) {
	case AF_INET: {
		struct ip_mreq		mreq;
		struct ifreq		ifreq;

		memcpy(&mreq.imr_multiaddr,
			   &((struct sockaddr_in *) sa)->sin_addr,
			   sizeof(struct in_addr));

		if (ifindex > 0) {
			if (if_indextoname(ifindex, ifreq.ifr_name) == NULL) {
				errno = ENXIO;	/* i/f index not found */
				return(-1);
			}
			goto doioctl;
		} else if (ifname != NULL) {
			strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
doioctl:
			if (ioctl(sockfd, SIOCGIFADDR, &ifreq) < 0)
				return(-1);
			memcpy(&mreq.imr_interface,
				   &((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr,
				   sizeof(struct in_addr));
		} else
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

		return(setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
						  &mreq, sizeof(mreq)));
	}

#ifdef	IPV6
	case AF_INET6: {
		struct ipv6_mreq	mreq6;

		memcpy(&mreq6.ipv6mr_multiaddr,
			   &((struct sockaddr_in6 *) sa)->sin6_addr,
			   sizeof(struct in6_addr));

		if (ifindex > 0)
			mreq6.ipv6mr_interface = ifindex;
		else if (ifname != NULL)
			if ( (mreq6.ipv6mr_interface = if_nametoindex(ifname)) == 0) {
				errno = ENXIO;	/* i/f name not found */
				return(-1);
			}
		else
			mreq6.ipv6mr_interface = 0;

		return(setsockopt(sockfd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP,
						  &mreq6, sizeof(mreq6)));
	}
#endif

	default:
		errno = EPROTONOSUPPORT;
		return(-1);
	}
}

int Mcast_join(int sockfd, const char *mcast_addr,
	       const char *ifname, u_int ifindex)
{ 
  #ifndef IPV6 
  /* IPv4 */
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  inet_pton(AF_INET, mcast_addr, &sa.sin_addr);
  #else
  struct sockaddr_in6 sa;
  sa.sin6_family = AF_INET6;
  inet_pton(AF_INET6, mcast_addr, &sa.sin6_addr);
  #endif

  return (mcast_join(sockfd, (struct sockaddr *) &sa, sizeof(sa), 
		     ifname, ifindex));
}

void sock_set_addr(struct sockaddr *sa, socklen_t salen, const void *addr)
{
  switch (sa->sa_family) {
  case AF_INET: {
    struct sockaddr_in	*sin = (struct sockaddr_in *) sa;
    
    memcpy(&sin->sin_addr, addr, sizeof(struct in_addr));
    return;
  }

  #ifdef IPV6
  case AF_INET6: {
    struct sockaddr_in6	*sin6 = (struct sockaddr_in6 *) sa;
	  
    memcpy(&sin6->sin6_addr, addr, sizeof(struct in6_addr));
    return;
  }
  #endif
  }

  return;
}

