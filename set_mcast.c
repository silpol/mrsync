/* 
   Copyright (C)  2006 Renaissance Technologies Corp.
                  main developer: HP Wei <hp@rentec.com>

		  This file collects functions related to setting multicast
		  for multicaster. They are IPv4 and IPv6 ready.
		  By default, we use IPv4.
		  To use IPv6, we need to specify -DIPv6 in Makefile.
		  The functions in this file are collected from
		  Richard Stevens' Networking bible: Unix Network programming

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
#define MAXSOCKADDR  128        /* max socket address structure size */

int sockfd_to_family(int sockfd)
{
        union {
          struct sockaddr       sa;
          char                          data[MAXSOCKADDR];
        } un;
        socklen_t       len;

        len = MAXSOCKADDR;
        if (getsockname(sockfd, (SA *) un.data, &len) < 0)
                return(-1);
        return(un.sa.sa_family);
}

int mcast_set_if(int sockfd, const char *ifname, u_int ifindex)
{
	switch (sockfd_to_family(sockfd)) {
	case AF_INET: {
		struct in_addr		inaddr;
		struct ifreq		ifreq;

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
			memcpy(&inaddr,
				   &((struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr,
				   sizeof(struct in_addr));
		} else
			inaddr.s_addr = htonl(INADDR_ANY);	/* remove prev. set default */

		return(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF,
						  &inaddr, sizeof(struct in_addr)));
	}

#ifdef	IPV6
	case AF_INET6: {
		u_int	index;

		if ( (index = ifindex) == 0) {
			if (ifname == NULL) {
				errno = EINVAL;	/* must supply either index or name */
				return(-1);
			}
			if ( (index = if_nametoindex(ifname)) == 0) {
				errno = ENXIO;	/* i/f name not found */
				return(-1);
			}
		}
		return(setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_IF,
						  &index, sizeof(index)));
	}
#endif

	default:
		errno = EPROTONOSUPPORT;
		return(-1);
	}
}

int mcast_set_loop(int sockfd, int onoff)
{
	switch (sockfd_to_family(sockfd)) {
	case AF_INET: {
		u_char		flag;

		flag = onoff;
		return(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_LOOP,
						  &flag, sizeof(flag)));
	}

#ifdef	IPV6
	case AF_INET6: {
		u_int		flag;

		flag = onoff;
		return(setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
						  &flag, sizeof(flag)));
	}
#endif

	default:
		errno = EPROTONOSUPPORT;
		return(-1);
	}
}
/* end mcast_set_loop */

int mcast_set_ttl(int sockfd, int val)
{
	switch (sockfd_to_family(sockfd)) {
	case AF_INET: {
		u_char		ttl;

		ttl = val;
		return(setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL,
						  &ttl, sizeof(ttl)));
	}

#ifdef	IPV6
	case AF_INET6: {
		int		hop;

		hop = val;
		return(setsockopt(sockfd, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
						  &hop, sizeof(hop)));
	}
#endif

	default:
		errno = EPROTONOSUPPORT;
		return(-1);
	}
}

