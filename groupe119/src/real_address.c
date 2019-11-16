#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>

#include "real_address.h"

const char * real_address(const char *address, struct sockaddr_in6 *rval) {
	struct addrinfo* addr;
	struct addrinfo hints;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_DGRAM;


	int err = getaddrinfo(address, NULL, NULL, &addr);
	if(err != 0) return gai_strerror(err);

	memcpy(rval, (struct sockaddr_in6 *) addr->ai_addr, addr->ai_addrlen);
	
	return NULL;
}
